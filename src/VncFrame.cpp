/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncFrame.h"

#include <qbuffer.h>
#include <qimage.h>
#include <qimagewriter.h>

namespace
{
    QByteArray encodedBytes( const QSize& size,
        const QByteArray& bytes, int quality )
    {
        QByteArray encodedBytes;
        QBuffer buffer( &encodedBytes );

        QImageWriter imageWriter( &buffer, "jpeg" );
        imageWriter.setQuality( quality );

        const QImage image(
            reinterpret_cast< const uint8_t* >( bytes.constData() ),
            size.width(), size.height(), QImage::Format_RGB32 );

        imageWriter.write( image );

        return encodedBytes;
    }

    QByteArray clippedBytes( const QSize& size,
        const QByteArray& bytes, const QRect& region )
    {
        const auto stride = region.width() * sizeof( QRgb );

        QByteArray clippedBytes( region.height() * stride, Qt::Uninitialized );

        auto from = reinterpret_cast< const QRgb* >( bytes.constData() );
        from += region.top() * size.width() + region.left();

        auto to = reinterpret_cast< QRgb* >( clippedBytes.data() );

        for ( int i = 0; i < region.height(); i++ )
        {
            memcpy( to, from, stride );

            from += size.width();
            to += region.width();
        }

        return clippedBytes;
    }
}

VncFrame::VncFrame()
{
}

VncFrame::VncFrame( Encoding encoding, const QRect& region, const QByteArray& bytes )
    : m_encoding( encoding )
    , m_region( region )
    , m_bytes( bytes )
{
}

void VncFrame::setFrame( Encoding encoding,
    const QRect& region, const QByteArray& bytes )
{
    m_encoding = encoding;
    m_region = region;
    m_bytes = bytes;
}

const uint8_t* VncFrame::bytes() const
{
    return reinterpret_cast< const uint8_t* >( m_bytes.constData() );
}

VncFrame VncFrame::encoded( int quality ) const
{
    Q_ASSERT( m_encoding == Rgb );

    if ( quality <= 0 )
        return *this;

    const auto bytes = encodedBytes( m_region.size(), m_bytes, quality );
    return VncFrame( Jpeg, m_region, bytes );
}

VncFrame VncFrame::clipped( const QRect& region ) const
{
    Q_ASSERT( m_encoding == Rgb );

    if ( region == m_region )
        return *this;

    const auto bytes = ::clippedBytes( m_region.size(), m_bytes, region );
    return VncFrame( VncFrame::Rgb, region, bytes );
}
