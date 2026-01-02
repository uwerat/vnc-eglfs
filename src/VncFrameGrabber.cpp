/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncFrameGrabber.h"
#include "VncFrame.h"

#ifdef VNC_VA_ENCODER
#include "va/VncVaApplication.h"
#endif

#include <qoffscreensurface.h>
#include <qopenglcontext.h>
#include <qopenglextrafunctions.h>

#include <qrect.h>
#include <qwaitcondition.h>
#include <qmutex.h>
#include <qatomic.h>
#include <qdebug.h>

namespace
{
    class FrameBufferObject : QOpenGLExtraFunctions
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

        QByteArray readPixels()
        {
            return readPixels( 0, 0, m_size.width(), m_size.height() );
        }

        QByteArray readPixels( int x, int y, int width, int height )
        {
            QByteArray bytes( width * height * 4, Qt::Uninitialized );

            glBindFramebuffer( GL_FRAMEBUFFER, m_fbo );

            glFramebufferTexture2D( GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureId, 0 );

            GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
            if ( status != GL_FRAMEBUFFER_COMPLETE )
                qDebug() << "FBO setup failed!";

            glReadPixels( x, y, width, height,
                GL_BGRA, GL_UNSIGNED_BYTE, bytes.data() );

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture( GL_TEXTURE_2D, 0 );

            return bytes;
        }

        bool isFunctional( const QOpenGLContext* context )
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

      private:
        GLuint m_textureId = 0;
        GLuint m_fbo = 0;

        QSize m_size;
    };
}

namespace
{
    class ThreadData
    {
      public:
        void setRequest( const QRect& region, int quality )
        {
            this->region = region;
            this->quality = quality;

            done = false;
            requested = true;
        }

        void setResult( const VncFrame& frame )
        {
            this->frame = frame;
            done = true;
        }

        QMutex mutex;
        QWaitCondition waitCondition;
        QAtomicInt abort {0};

        QRect region;
        int quality = 0;
        VncFrame frame;

        bool done = false;
        bool requested = false;
   };
}

class VncFrameGrabber::PrivateData
{
  public:
    QOpenGLContext* context = nullptr;

    FrameBufferObject fbo;
    ThreadData threadData;

    bool videoAcceleration = false;
};

VncFrameGrabber::VncFrameGrabber( QObject* parent )
    : QThread(parent)
    , m_data( new PrivateData() )
{
    m_data->context = QOpenGLContext::currentContext();
    
#ifdef VNC_VA_ENCODER
    m_data->videoAcceleration = VncVaApplication::isValid();
#endif

    Q_ASSERT( m_data->fbo.isFunctional( m_data->context ) );

    start(); // launch the thread
}

VncFrameGrabber::~VncFrameGrabber()
{
    {
        auto& td = m_data->threadData;

        QMutexLocker locker( &td.mutex );
        td.abort.storeRelease( true );
        td.waitCondition.wakeAll();
    }

    wait(); // wait for thread to exit
}

bool VncFrameGrabber::supportsVideoAcceleration() const
{
    return m_data->videoAcceleration;
}

void VncFrameGrabber::importBackBuffer( const QSize& size )
{
    m_data->fbo.resize( size );
    m_data->fbo.importBackBuffer();
}

VncFrame VncFrameGrabber::grabFrame( const QRect& region, const int quality )
{
    auto& td = m_data->threadData;

    QMutexLocker locker( &td.mutex );

    td.setRequest( region, quality );

    td.waitCondition.wakeOne();
    while ( !td.done )
        td.waitCondition.wait( &td.mutex );

    return td.frame;
}

void VncFrameGrabber::run()
{
    QSurfaceFormat fmt;
    fmt.setRenderableType( QSurfaceFormat::OpenGLES );
    fmt.setVersion(2,0);

    QOpenGLContext contextES;
    contextES.setShareContext( m_data->context );
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

    VncVaApplication encoder( m_data->fbo.textureId() );

    auto& td = m_data->threadData;

    while ( !td.abort.loadAcquire() )
    {
        QRect region;
        int quality;

        {
            QMutexLocker locker( &td.mutex );
            if ( !td.requested )
            {
                // wait for a job
                td.waitCondition.wait( &td.mutex );
                continue;
            }

            region = td.region;
            quality = td.quality;

            td.requested = false;
        }

        VncFrame frame;

        contextES.makeCurrent( &offscreen );

        auto& fbo = m_data->fbo;

        if ( quality <= 0 )
        {
            const auto data = fbo.readPixels();
            frame.setFrame( VncFrame::Rgb, QRect( QPoint(), fbo.size() ), data );
        }
        else
        {
            const auto data = encoder.encode( fbo.size(), region, quality );
            frame.setFrame( VncFrame::Jpeg, region, data );
        }

        contextES.doneCurrent();

        {
            QMutexLocker locker( &td.mutex );
            td.setResult( frame );
            td.waitCondition.wakeOne();
        }
    }
}
