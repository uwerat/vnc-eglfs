/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncTextureGrabber.h"

#ifdef VNC_VA_ENCODER
#include "va/VncVaApplication.h"
#endif

#include <qoffscreensurface.h>
#include <qopenglcontext.h>
#include <qopenglextrafunctions.h>

class VncTextureGrabber::FrameBufferObject : QOpenGLExtraFunctions
{
  public:
    FrameBufferObject()
    {
        initializeOpenGLFunctions();

        glGenTextures( 1, &m_textureId );

        glBindTexture( GL_TEXTURE_2D, m_textureId );
        glGenFramebuffers( 1, &m_fbo );
        glBindTexture( GL_TEXTURE_2D, 0 );

        glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_fbo );
        glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureId, 0 );
        glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
    }

    ~FrameBufferObject()
    {
        glDeleteFramebuffers( 1, &m_fbo );
        glDeleteTextures( 1, &m_textureId );
    }

    QSize size() const { return m_size; }
    GLuint textureId() const { return m_textureId; }

    void resize( const QSize& size )
    {
        if ( size != m_size )
        {
            glBindTexture( GL_TEXTURE_2D, m_textureId );

            glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8,
                size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            glBindTexture( GL_TEXTURE_2D, 0 );

            m_size = size;
        }
    }

    void importBackBuffer()
    {
        const int w = m_size.width(); 
        const int h = m_size.height();

        glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_fbo );

        glReadBuffer( GL_BACK );
        glBlitFramebuffer( 0, 0, w, h, 0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST ); 

        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
        glBindTexture( GL_TEXTURE_2D, 0 );
    }

    void readPixels( uint8_t* data  )
    {
        glBindFramebuffer( GL_FRAMEBUFFER, m_fbo );

        glFramebufferTexture2D( GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureId, 0 );

        GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
        if ( status != GL_FRAMEBUFFER_COMPLETE )
            qDebug() << "FBO setup failed!";

        glReadPixels( 0, 0, m_size.width(), m_size.height(),
            GL_BGRA, GL_UNSIGNED_BYTE, data );

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture( GL_TEXTURE_2D, 0 );
    }

  private:
    GLuint m_textureId = 0;
    GLuint m_fbo = 0;

    QSize m_size;
};

bool VncTextureGrabber::isSupported( const QOpenGLContext* context )
{
    /*
        - glReadBuffer
            no problem as QOpenGLExtraFunctions does a nop
            and we run into the default ( = GL_BACK ) then.

        - glBlitFramebuffer
            we could use glCopyTexImage2D and flip somewhere else.
     */
    if ( context->format().majorVersion() < 3 ) 
    {
        return context->hasExtension("GL_EXT_framebuffer_blit")
            || context->hasExtension("GL_NV_framebuffer_blit");
    }

    return true;
}

VncTextureGrabber::VncTextureGrabber( QObject* parent )
    : QThread(parent)
    , m_context( QOpenGLContext::currentContext() )
    , m_fbo( new FrameBufferObject() )
{
    start(); // launch the thread
}

VncTextureGrabber::~VncTextureGrabber()
{
    {
        QMutexLocker locker( &m_mutex );
        m_abort.storeRelease( true );
        m_waitCondition.wakeAll();
    }

    wait(); // wait for thread to exit
    delete m_fbo;
}

void VncTextureGrabber::importBackBuffer( const QSize& size )
{
    m_fbo->resize( size );
    m_fbo->importBackBuffer();
}

VncFrame VncTextureGrabber::grabFrame( const QRect& subRect, const int quality )
{
    QMutexLocker locker( &m_mutex );

    m_subRect = subRect;
    m_quality = quality;

    m_done = false;
    m_requested = true;

    // notify thread
    m_waitCondition.wakeOne();

    // wait for completion
    while ( !m_done )
        m_waitCondition.wait( &m_mutex );

    return m_frame;
}

void VncTextureGrabber::run()
{
    QSurfaceFormat fmt;
    fmt.setRenderableType( QSurfaceFormat::OpenGLES );
    fmt.setVersion(2,0);
    
    QOpenGLContext contextES;
    contextES.setShareContext( m_context );
    contextES.setFormat( fmt );
    contextES.create();
            
    QOffscreenSurface offscreen;
    offscreen.setFormat(fmt);
    offscreen.create();

    if ( !contextES.makeCurrent( &offscreen ) )
    {
        qWarning("Failed to make worker context current");
        return;
    }

    auto f = *contextES.functions();

    f.initializeOpenGLFunctions();

    VncVaApplication encoder;

    while ( !m_abort.loadAcquire() )
    {
        QRect subRect;
        int quality;

        {
            QMutexLocker locker( &m_mutex );
            if ( !m_requested )
            {
                // wait for a job
                m_waitCondition.wait( &m_mutex );
                continue;
            }

            subRect = m_subRect;
            quality = m_quality;

            m_requested = false;
        }

        VncFrame frame;

        contextES.makeCurrent( &offscreen );

        if ( quality <= 0 )
        {
            VncFrame rgbFrame( VncFrame::Rgb, m_fbo->size() );
            m_fbo->readPixels( rgbFrame.editableBytes() );

            frame = rgbFrame;
        }
        else
        {
            encoder.open();

            const auto data = encoder.encode(
                m_fbo->textureId(), m_fbo->size(), subRect, quality );

            frame = VncFrame::fromByteArray( VncFrame::Jpeg,
                subRect.width(), subRect.height(), data );
        }

        contextES.doneCurrent();

        {
            QMutexLocker locker(&m_mutex);
            m_done = true;
            m_frame = frame;
            m_waitCondition.wakeOne();
        }
    }
}
