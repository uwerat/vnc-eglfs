/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncFrame.h"
#include <qcolor.h>

static inline qsizetype byteCount(
    VncFrame::Encoding encoding, int width, int height )
{
    qsizetype count = width * height;
    if ( encoding == VncFrame::Rgb )
        count *= 4;

    return count;
}

VncFrame::VncFrame( Encoding encoding,
        int width, int height, const uint8_t* bytes )
    : m_encoding( encoding )
    , m_width( width )
    , m_height( height )
    , m_bytes( reinterpret_cast< const char* >( bytes ),
        ::byteCount( encoding, width, height ) )
{
}

VncFrame::VncFrame( Encoding encoding, int width, int height )
    : m_encoding( encoding )
    , m_width( width )
    , m_height( height )
    , m_bytes( ::byteCount( encoding, width, height ), Qt::Uninitialized )
{
}

VncFrame::VncFrame( Encoding encoding, int width, int height, const QByteArray& bytes )
    : m_encoding( encoding )
    , m_width( width )
    , m_height( height )
    , m_bytes( bytes )
{
}

const uint8_t* VncFrame::bytes() const
{
    return reinterpret_cast< const uint8_t* >( m_bytes.constData() );
}

uint8_t* VncFrame::editableBytes()
{
    return reinterpret_cast< uint8_t* >( m_bytes.data() );
}

VncFrame VncFrame::fromRawData( Encoding encoding,
    int width, int height, const uint8_t* bytes )
{
    const auto count = ::byteCount( encoding, width, height );
    const auto b = reinterpret_cast< const char* > ( bytes );

    return VncFrame( encoding, width, height,
        QByteArray::fromRawData( b, count ) );
}

VncFrame VncFrame::fromByteArray( Encoding encoding,
    int width, int height, const QByteArray& bytes )
{
    return VncFrame( encoding, width, height, bytes );
}

VncFrame VncFrame::subFrame( const QRect& section ) const
{
    const auto rect = boundingRect();

    if ( section == rect )
        return *this;

    Q_ASSERT( rect.contains( section ) );
    Q_ASSERT( m_encoding == VncFrame::Rgb );

    if ( m_encoding != VncFrame::Rgb || !rect.contains( section ) )
        return VncFrame();

    VncFrame subFrame( m_encoding, section.size() );

    auto from = reinterpret_cast< const QRgb* >( m_bytes.constData() );
    from += section.top() * m_width + section.left();

    auto to = reinterpret_cast< QRgb* >( subFrame.m_bytes.data() );

    const auto lineCount = section.width() * sizeof( QRgb );

    for ( int i = 0; i < height(); i++ )
    {
        memcpy( to, from, lineCount );

        from += width();
        to += section.width();
    }

    return subFrame;
}

void VncFrame::reset()
{
    *this = VncFrame();
}
