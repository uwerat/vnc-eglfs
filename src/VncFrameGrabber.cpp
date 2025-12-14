/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncFrameGrabber.h"
#include "VncTextureGrabber.h"

#ifdef VNC_VA_ENCODER
#include "va/VncVaEncoder.h"
#endif

#include <qrect.h>
#include <qmutex.h>

#include <qopenglcontext.h>
#include <qopenglfunctions.h>
#include <qopenglextrafunctions.h>
#include <qimage.h>
#include <qbuffer.h>
#include <qimagewriter.h>

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

class VncFrameGrabber::PrivateData
{
  public:
    VncTextureGrabber* textureGrabber = nullptr;
    QOpenGLContext* context = nullptr;
    GLuint textureId = 0;

    QSize size;

    mutable VncFrame rgbFrame;
    mutable QHash< int, VncFrame > jpegFrames;
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
            m_data->textureGrabber = new VncTextureGrabber( m_data->context );
    }

    m_data->size = size;
    fillTexture( m_data->context, m_data->textureId, size );

    m_data->rgbFrame.reset();
    m_data->jpegFrames.clear();
}

VncFrame VncFrameGrabber::frame(
    VncFrame::Encoding encoding, int qualityLevel ) const
{
    const QRect r( 0, 0, m_data->size.width(), m_data->size.height() );
    return subFrame( r, encoding, qualityLevel );
}

VncFrame VncFrameGrabber::subFrame(
    const QRect& subRect, VncFrame::Encoding encoding, int qualityLevel ) const
{
    if ( encoding == VncFrame::Rgb )
    {
        auto& frame = m_data->rgbFrame;
        if ( !frame.isValid() )
        {
            frame = m_data->textureGrabber->grabFrame(
                m_data->textureId, m_data->size, subRect, 0 );
        }

        return frame.subFrame( subRect );
    }

    if ( encoding == VncFrame::Jpeg )
    {
        /*
            quality: [1:100], level: [0,9].
            Higher means better quality + less compression
         */

        const auto quality = ( qualityLevel + 1 ) * 10;

        auto& frame = m_data->jpegFrames[ qHash( subRect, quality ) ];

        if ( !frame.isValid() )
        {
            const bool useVideoAcceleration = true;

            if ( useVideoAcceleration )
            {
                frame = m_data->textureGrabber->grabFrame(
                    m_data->textureId, m_data->size, subRect, quality );
            }
            else
            {
                auto frm = subFrame( subRect, VncFrame::Rgb, 0 );
                frm = frm.subFrame( subRect );

                const auto bytes = frameToJPEG( frm, quality );

                frame = VncFrame::fromByteArray( encoding,
                    frm.width(), frm.height(), bytes );
            }
        }

        return frame;
    }

    return VncFrame();
}

void VncFrameGrabber::invalidate()
{
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

    m_data->rgbFrame.reset();
    m_data->jpegFrames.clear();
}
