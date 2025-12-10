/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncTextureGrabber.h"

#ifdef VNC_VA_ENCODER
#include "VncDmaBuffer.h"
#include "va/VncVaEncoder.h"
#endif

#include <qopenglcontext.h>
#include <QOpenGLExtraFunctions>

#include <qsurface.h>
#include <qoffscreensurface.h>
#include <qelapsedtimer.h>

#include <EGL/egl.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <fcntl.h>
#include <unistd.h>

static VncFrame grabTexture( GLuint textureId, const QSize& size )
{
    VncFrame frame( VncFrame::Rgb, size );

    glBindTexture( GL_TEXTURE_2D, textureId );

    const GLenum glFormat = GL_BGRA;
#if 0
    glGetTexImage( GL_TEXTURE_2D, 0, glFormat,
        GL_UNSIGNED_BYTE, frame.editableBytes() );
#else
    // Create a temporary framebuffer
    GLuint fbo = 0;
    glGenFramebuffers( 1, &fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );

    // Attach the texture
    glFramebufferTexture2D( GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0 );

    // Check FBO completeness
    GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
    if ( status != GL_FRAMEBUFFER_COMPLETE )
        qDebug() << "FBO setup failed!";

    // Read pixels from the framebuffer
    glReadPixels( 0, 0, size.width(), size.height(),
        glFormat, GL_UNSIGNED_BYTE, frame.editableBytes() );

    // Clean up framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
#endif

    glBindTexture( GL_TEXTURE_2D, 0 );

    return frame;
}

VncTextureGrabber::VncTextureGrabber( QOpenGLContext* context, QObject* parent )
    : QThread(parent)
    , m_context( context )
    , m_encoder( new VncVaEncoder() )
{
    start(); // launch the thread
}

VncTextureGrabber::~VncTextureGrabber()
{
    {
        QMutexLocker locker( &m_mutex );
        m_abort.storeRelease(true);
        m_waitCondition.wakeAll();
    }

    wait(); // wait for thread to exit

    delete m_encoder;
}

VncFrame VncTextureGrabber::grabFrame( uint textureId,
    const QSize& size, const QRect& subRect, const int quality )
{
    QMutexLocker locker( &m_mutex );

    m_textureId = textureId;
    m_size = size;
    m_subRect = subRect;
    m_quality = quality;

    m_done = false;

    // notify thread
    m_waitCondition.wakeOne();

    // wait for completion
    while ( !m_done )
        m_waitCondition.wait( &m_mutex );

    return m_frame;
}

VncFrame VncTextureGrabber::encodeFrame( const VncFrame& frame, int quality )
{
    m_encoder->open();
    m_encoder->setFrame( frame );

    return m_encoder->encode( QRect(), quality );
}

VncFrame VncTextureGrabber::encodeFrame(
    uint textureId, const QSize& size, const QRect& subRect, int quality )
{
    m_encoder->open();
    m_encoder->setFrame( VncDmaBuffer( size, textureId ) );

    return m_encoder->encode( subRect, quality );
}

void VncTextureGrabber::run()
{
    QOpenGLContext contextGL;
    contextGL.setShareContext( m_context );
    contextGL.setFormat( m_context->format() );

    if (!contextGL.create() )
    {
        qWarning("Failed to create worker OpenGL context");
        return;
    }

    QOffscreenSurface offscreen;
    offscreen.setFormat( contextGL.format() );
    offscreen.create();

    if ( !contextGL.makeCurrent( &offscreen ) )
    {
        qWarning("Failed to make worker context current");
        return;
    }

    contextGL.functions()->initializeOpenGLFunctions();

    m_encoder->open();

    while ( !m_abort.loadAcquire() )
    {
        GLuint textureId;
        QSize size;
        QRect subRect;
        int quality;

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
            subRect = m_subRect;
            quality = m_quality;

            // mark job claimed
            m_textureId = 0;
        }

        VncFrame frame;

        contextGL.makeCurrent( &offscreen );

        {
            QElapsedTimer timer;
            timer.start();

            if ( quality <= 0 )
                frame = grabTexture( textureId, size );
            else
                frame = encodeFrame( textureId, size, subRect, quality );

#if 0
            qDebug() << 0 << frame.byteCount() << timer.elapsed();
#endif
        }

        contextGL.doneCurrent();

        {
            QMutexLocker locker(&m_mutex);
            m_done = true;
            m_frame = frame;
            m_waitCondition.wakeOne();
        }
    }

    m_encoder->close();
}

