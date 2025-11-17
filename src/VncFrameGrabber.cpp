/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncFrameGrabber.h"

#ifdef VNC_VA_ENCODER
#include "va/VncVaEncoder.h"
#endif

#include <qrect.h>
#include <qmutex.h>
#include <qthread.h>

#include <qopenglcontext.h>
#include <qopenglfunctions.h>
#include <QOpenGLExtraFunctions>
#include <qsurface.h>
#include <qimage.h>
#include <qbuffer.h>
#include <qimagewriter.h>

#include <QWaitCondition>
#include <QAtomicInt>
#include <QOffscreenSurface>

static void fillTexture( QOpenGLContext* context,
    const unsigned int textureId, const QSize& size )
{
    const int width = size.width();
    const int height = size.height();

    auto& f = *context->functions();

    f.glBindTexture( GL_TEXTURE_2D, textureId );
    f.glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8,
        size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    f.glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    f.glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

    GLuint fbo;
    f.glGenFramebuffers( 1, &fbo );
    f.glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );

    f.glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0 );

    if ( f.glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
        qDebug() << "FBO setup failed!";

    f.glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
    context->extraFunctions()->glReadBuffer( GL_BACK );

    {
        typedef void ( QOPENGLF_APIENTRYP PFNGLBLITFRAMEBUFFERPROC )(
            GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);

        auto blitFBO = (PFNGLBLITFRAMEBUFFERPROC) context->getProcAddress("glBlitFramebuffer");

        blitFBO( 0, 0, width, height,
            0, height, width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST ); // copy + flip
    }

    f.glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    f.glDeleteFramebuffers( 1, &fbo );
}

static VncFrame grabTexture( QOpenGLFunctions& f, GLuint textureId, const QSize& size )
{
    VncFrame frame( VncFrame::Rgb, size );

    f.glBindTexture( GL_TEXTURE_2D, textureId );

    const GLenum glFormat = GL_BGRA;
#if 0
    f.glGetTexImage( GL_TEXTURE_2D, 0, glFormat,
        GL_UNSIGNED_BYTE, frame.editableBytes() );
#else
    // Create a temporary framebuffer
    GLuint fbo = 0;
    f.glGenFramebuffers( 1, &fbo );
    f.glBindFramebuffer( GL_FRAMEBUFFER, fbo );

    // Attach the texture
    f.glFramebufferTexture2D( GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0 );

    // Check FBO completeness
    GLenum status = f.glCheckFramebufferStatus( GL_FRAMEBUFFER );
    if ( status != GL_FRAMEBUFFER_COMPLETE )
        qDebug() << "FBO setup failed!";

    // Read pixels from the framebuffer
    f.glReadPixels( 0, 0, size.width(), size.height(),
        glFormat, GL_UNSIGNED_BYTE, frame.editableBytes() );

    // Clean up framebuffer
    f.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    f.glDeleteFramebuffers(1, &fbo);
#endif

    f.glBindTexture( GL_TEXTURE_2D, 0 );

    return frame;
}

static QByteArray frameToJPEG( const VncFrame& frame, int quality )
{
    QByteArray data;
    QBuffer buffer( &data );

    QImageWriter imageWriter( &buffer, "jpeg" );
    imageWriter.setQuality( quality );

    const QImage image( frame.bytes(),
        frame.width(), frame.height(), QImage::Format_RGB32 );

    imageWriter.write( image );

    return data;
}

namespace
{
    /*
        Downloading the texture does not happen in the scene graph thread
        and we can't use its context. So we need an extra thread holding
        a shared context and an offline surface.
     */

    class TextureGrabber : public QThread
    {
      public:
        TextureGrabber( QOpenGLContext*, QObject* parent = nullptr );
        ~TextureGrabber() override;

        VncFrame grabFrame( GLuint textureId, const QSize& );

      protected:
        void run() override;

      private:
        QOpenGLContext* m_context = nullptr;

        QMutex m_mutex;
        QWaitCondition m_waitCondition;
        QAtomicInt m_abort {0};

        GLuint m_textureId = 0;
        QSize m_size;
        VncFrame m_frame;
        bool m_done = false;
    };

    TextureGrabber::TextureGrabber( QOpenGLContext* context, QObject* parent )
        : QThread(parent)
        , m_context( context )
    {
        start(); // launch the thread
    }

    TextureGrabber::~TextureGrabber()
    {
        {
            QMutexLocker locker( &m_mutex );
            m_abort.storeRelease(true);
            m_waitCondition.wakeAll();
        }

        wait(); // wait for thread to exit
    }

    VncFrame TextureGrabber::grabFrame( GLuint textureId, const QSize& size )
    {
        QMutexLocker locker( &m_mutex );

        m_textureId = textureId;
        m_size = size;
        m_done = false;

        // notify thread
        m_waitCondition.wakeOne();

        // wait for completion
        while ( !m_done )
            m_waitCondition.wait( &m_mutex );

        return m_frame;
    }

    void TextureGrabber::run()
    {
        QOpenGLContext ctx;
        ctx.setShareContext( m_context );
        ctx.setFormat( m_context->format() );

        if (!ctx.create() )
        {
            qWarning("Failed to create worker OpenGL context");
            return;
        }

        QOffscreenSurface offscreen;
        offscreen.setFormat( ctx.format() );
        offscreen.create();

        if ( !ctx.makeCurrent( &offscreen ) )
        {
            qWarning("Failed to make worker context current");
            return;
        }

        auto& f = *ctx.functions();
        f.initializeOpenGLFunctions();

        while ( !m_abort.loadAcquire() )
        {
            GLuint textureId;
            QSize size;

            {
                QMutexLocker locker( &m_mutex );
                if ( m_textureId == 0 )
                {
                    // wait for a job
                    m_waitCondition.wait( &m_mutex );
                    continue;
                }

                textureId = m_textureId;
                size = m_size;

                // mark job claimed
                m_textureId = 0;
            }

            const auto frame = grabTexture( f, textureId, size );

            {
                QMutexLocker locker(&m_mutex);
                m_done = true;
                m_frame = frame;
                m_waitCondition.wakeOne();
            }
        }

        ctx.doneCurrent();
    }
}

class VncFrameGrabber::PrivateData
{
  public:
    QMutex mutex;

    TextureGrabber* textureGrabber = nullptr;
    QOpenGLContext* context = nullptr;
    GLuint textureId = 0;

#ifdef VNC_VA_ENCODER
    mutable VncVaEncoder encoder;
#endif

    QSize size;

    mutable VncFrame pixels;
    mutable QHash< int, VncFrame > jpegTiles;
};

VncFrameGrabber::VncFrameGrabber( QObject* parent )
    : QObject( parent )
    , m_data( new PrivateData )
{
}

VncFrameGrabber::~VncFrameGrabber()
{
    invalidate();
}

bool VncFrameGrabber::isValid() const
{
    QMutexLocker locker( &m_data->mutex );
    return !m_data->size.isEmpty();
}

QSize VncFrameGrabber::frameSize() const
{
    return m_data->size;
}

void VncFrameGrabber::update( const QSize& size )
{
    if ( m_data->textureId == 0 )
    {
        m_data->context = QOpenGLContext::currentContext();
        m_data->context->extraFunctions()->initializeOpenGLFunctions();

        m_data->context->functions()->glGenTextures( 1, &m_data->textureId );

        if ( m_data->textureGrabber == nullptr )
            m_data->textureGrabber = new TextureGrabber( m_data->context );
    }

    Q_ASSERT( size == m_data->context->surface()->size() );

    QMutexLocker locker( &m_data->mutex );

    m_data->size = size;
    fillTexture( m_data->context, m_data->textureId, size );

    m_data->pixels.reset();
    m_data->jpegTiles.clear();
}

VncFrame VncFrameGrabber::frame(
    VncFrame::Encoding encoding, int qualityLevel ) const
{
    const QRect r( 0, 0, m_data->size.width(), m_data->size.height() );
    return subFrame( r, encoding, qualityLevel );
}

VncFrame VncFrameGrabber::subFrame(
    const QRect& rect, VncFrame::Encoding encoding, int qualityLevel ) const
{
    QMutexLocker locker( &m_data->mutex );

    bool useVA = true;

#if 0
    if ( format == Pixels || !useVA )
#endif
    {
        if ( !m_data->pixels.isValid() )
        {
            m_data->pixels = m_data->textureGrabber->grabFrame(
                m_data->textureId, m_data->size );
        }
    }


    if ( encoding == VncFrame::Rgb )
    {
        return m_data->pixels.subFrame( rect );
    }

    auto& frame = m_data->jpegTiles[ qHash( rect, qualityLevel ) ];

    if ( !frame.isValid() )
    {
        /*
            quality: [1:100], level: [0,9].
            Higher means better quality + less compression
         */

        const auto quality = ( qualityLevel + 1 ) * 10;
        auto subFrame = m_data->pixels.subFrame( rect );

        if ( useVA )
        {
            m_data->encoder.open();
            frame = m_data->encoder.encodeJPG( subFrame, quality );
        }
        else
        {
            const auto bytes = frameToJPEG( subFrame, quality );

            frame = VncFrame::fromByteArray( encoding,
                subFrame.width(), subFrame.height(), bytes );
        }
    }

    return frame;
}

void VncFrameGrabber::invalidate()
{
    QMutexLocker locker( &m_data->mutex );

    delete m_data->textureGrabber;
    m_data->textureGrabber = nullptr;

    if ( auto textureId = m_data->textureId )
    {
        m_data->textureId = 0;

        delete m_data->textureGrabber;

        auto& f = *m_data->context->functions();
        f.glDeleteTextures( 1, &textureId );
    }

    m_data->size = QSize();

    m_data->pixels.reset();
    m_data->jpegTiles.clear();
}
