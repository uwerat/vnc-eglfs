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
#include <qhash.h>

#include <qimage.h>
#include <qbuffer.h>
#include <qimagewriter.h>

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

class VncFrameGrabber::Cache
{
  public:
    void clear() { m_jpegFrames.clear(); }

    VncFrame& frame( int hash = 0 ) { return m_jpegFrames[ hash ]; }
    VncFrame& frame( const QRect& rect, int quality )
        { return frame( qHash( rect, quality ) ); }

  private:
    QHash< int, VncFrame > m_jpegFrames;
};

VncFrameGrabber::VncFrameGrabber( QObject* parent )
    : QObject( parent )
{
    m_cache = new Cache();
}

VncFrameGrabber::~VncFrameGrabber()
{
    invalidate();
    delete m_cache;
}

bool VncFrameGrabber::isValid() const
{
    return !m_size.isEmpty();
}

QSize VncFrameGrabber::frameSize() const
{
    return m_size;
}

void VncFrameGrabber::update( const QSize& size )
{
    if ( m_textureGrabber == nullptr )
        m_textureGrabber = new VncTextureGrabber();

    m_size = size;

    m_textureGrabber->importBackBuffer( size );
    m_cache->clear();
}

VncFrame VncFrameGrabber::frame(
    VncFrame::Encoding encoding, int qualityLevel ) const
{
    const QRect r( 0, 0, m_size.width(), m_size.height() );
    return subFrame( r, encoding, qualityLevel );
}

VncFrame VncFrameGrabber::subFrame(
    const QRect& subRect, VncFrame::Encoding encoding, int qualityLevel ) const
{
    if ( encoding == VncFrame::Rgb )
    {
        auto& frame = m_cache->frame();
        if ( !frame.isValid() )
            frame = m_textureGrabber->grabFrame( subRect, 0 );

        return frame.subFrame( subRect );
    }

    if ( encoding == VncFrame::Jpeg )
    {
        /*
            quality: [1:100], level: [0,9].
            Higher means better quality + less compression
         */

        const auto quality = ( qualityLevel + 1 ) * 10;

        auto& frame = m_cache->frame( subRect, quality );

        if ( !frame.isValid() )
        {
#ifdef VNC_VA_ENCODER
            static bool useVideoAcceleration = VncVaEncoder::isValid();
#else
            bool useVideoAcceleration = false;
#endif

            if ( useVideoAcceleration )
            {
                frame = m_textureGrabber->grabFrame( subRect, quality );
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
    delete m_textureGrabber;
    m_textureGrabber = nullptr;

    m_size = QSize();

    m_cache->clear();
}
