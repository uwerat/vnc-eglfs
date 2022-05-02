/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "RfbPixelStreamer.h"
#include "RfbSocket.h"

#include <qimage.h>
#include <qendian.h>

namespace
{
    class PixelFormat
    {
      public:
        inline constexpr bool isDefault() const noexcept
        {
            constexpr PixelFormat other;

            return ( m_bitsPerPixel == other.m_bitsPerPixel )
                && ( m_depth == other.m_depth )
                && ( m_bigEndian == other.m_bigEndian )
                && ( m_trueColor == other.m_trueColor )
                && ( m_redBits == other.m_redBits )
                && ( m_greenBits == other.m_greenBits )
                && ( m_blueBits == other.m_blueBits )
                && ( m_redShift == other.m_redShift )
                && ( m_greenShift == other.m_greenShift )
                && ( m_blueShift == other.m_blueShift );
        }

        void read( RfbSocket* socket )
        {
            socket->receivePadding( 3 );

            m_bitsPerPixel = socket->receiveUint8();
            m_depth = socket->receiveUint8();
            m_bigEndian = socket->receiveUint8();
            m_trueColor = socket->receiveUint8();

            m_redBits = bitCount( socket->receiveUint16() );
            m_greenBits = bitCount( socket->receiveUint16() );
            m_blueBits = bitCount( socket->receiveUint16() );

            m_redShift = socket->receiveUint8();
            m_greenShift = socket->receiveUint8();
            m_blueShift = socket->receiveUint8();

            socket->receivePadding( 3 );
        }

        void write( RfbSocket* socket )
        {
            socket->sendUint8( m_bitsPerPixel );
            socket->sendUint8( m_depth );
            socket->sendUint8( m_bigEndian );
            socket->sendUint8( m_trueColor );

            socket->sendUint16( bitMask( m_redBits ) );
            socket->sendUint16( bitMask( m_greenBits ) );
            socket->sendUint16( bitMask( m_blueBits ) );

            socket->sendUint8( m_redShift );
            socket->sendUint8( m_greenShift );
            socket->sendUint8( m_blueShift );

            socket->sendPadding( 3 );
        }

        void convertBuffer( const QRgb* from, int count, char* to ) const
        {
            switch( m_bitsPerPixel )
            {
                case 8:
                {
                    auto out = reinterpret_cast< quint8* >( to );
                    convertPixels( from, count, out );

                    break;
                }
                case 16:
                {
                    auto out = reinterpret_cast< quint16* >( to );
                    convertPixels( from, count, out );

                    break;
                }
                case 32:
                {
                    auto out = reinterpret_cast< quint32* >( to );
                    convertPixels( from, count, out );

                    break;
                }
            }
        }

        inline int bytesPerPixel() const
        {
            return m_bitsPerPixel / 8;
        }

        inline bool isTrueColor() const
        {
            return m_trueColor;
        }

      private:
        template< typename T >
        inline void convertPixels( const QRgb* rgbBuffer, int count, T* out ) const
        {
            for ( int i = 0; i < count; ++i )
            {
                const auto rgb = rgbBuffer[i];

                const int r = qRed( rgb ) >> ( 8 - m_redBits );
                const int g = qGreen( rgb ) >> ( 8 - m_greenBits );
                const int b = qBlue( rgb ) >> ( 8 - m_blueBits );

                const T pixel = ( r << m_redShift ) |
                    ( g << m_greenShift ) |
                    ( b << m_blueShift );

                if ( m_bigEndian )
                    qToBigEndian( pixel, out + i );
                else
                    qToLittleEndian( pixel, out + i );
            }
        }

        inline int bitCount( quint16 mask ) const
        {
            return qPopulationCount( mask );
        }

        inline quint16 bitMask( int count ) const
        {
            return static_cast< quint16 >( ( 1 << count ) - 1 );
        }

      private:
        int m_bitsPerPixel = 32;

        int m_depth = 24;
        bool m_bigEndian = ( QSysInfo::ByteOrder == QSysInfo::BigEndian );

        bool m_trueColor = true;

        int m_redBits = 8;
        int m_greenBits = 8;
        int m_blueBits = 8;

        int m_redShift = 16;
        int m_greenShift = 8;
        int m_blueShift = 0;
    };
}

class RfbPixelStreamer::PrivateData
{
  public:
    PixelFormat format;
};

RfbPixelStreamer::RfbPixelStreamer()
    : m_data( new PrivateData() )
{
}

RfbPixelStreamer::~RfbPixelStreamer()
{
    delete m_data;
}

void RfbPixelStreamer::sendServerFormat( RfbSocket* socket )
{
    PixelFormat().write( socket );
}

void RfbPixelStreamer::receiveClientFormat( RfbSocket* socket )
{
    m_data->format.read( socket );

    if ( !m_data->format.isTrueColor() )
        qWarning("VNC: can only handle true color clients");
}

void RfbPixelStreamer::sendImageData(
    const QImage& image, const QRect& rect, RfbSocket* socket )
{
    auto line = reinterpret_cast< const QRgb* >( image.constBits() );
    line += rect.y() * rect.width() + rect.x();

    const auto& format = m_data->format;

    if ( format.isDefault() )
    {
        for ( int i = 0; i < rect.height(); ++i )
        {
            socket->sendScanLine32( line, rect.width() );
            line += image.width();
        }
    }
    else
    {
        const int count = rect.width() * format.bytesPerPixel();
        QVarLengthArray< char > buffer( count );

        for ( int i = 0; i < rect.height(); ++i )
        {
            format.convertBuffer( line, rect.width(), buffer.data() );
            socket->sendScanLine8( buffer.constData(), buffer.size() );

            line += image.width();
        }
    }
}

void RfbPixelStreamer::sendImageRaw(
    const QImage& image, const QRegion& region, RfbSocket* socket )
{
    if ( region.isEmpty() )
        return;

    socket->sendUint8( 0 ); // msg type
    socket->sendPadding( 1 );

    socket->sendUint16( region.rectCount() );

    for ( const QRect& rect : region )
    {
        socket->sendRect64( rect );

        socket->sendEncoding32( 0 ); // Raw
        sendImageData( image, rect, socket );
    }

    socket->flush();
}

void RfbPixelStreamer::sendCursor(
    const QPoint& pos, const QImage& cursor, RfbSocket* socket )
{
    socket->sendUint8( 0 );
    socket->sendPadding( 1 );

    socket->sendUint16( 1 ); // number of rectangles
    socket->sendRect64( pos, cursor.size() );

    socket->sendEncoding32( -239 ); // Cursor

    {
        const auto image = cursor.convertToFormat( QImage::Format_RGB32 );

        const QRect r( 0, 0, image.width(), image.height() );
        sendImageData( image, r, socket );
    }

    {
        const auto mask = cursor.createAlphaMask();
        const auto bitmap = mask.convertToFormat( QImage::Format_Mono );

        const int width = ( bitmap.width() + 7 ) / 8;
        for ( int i = 0; i < bitmap.height(); ++i )
            socket->sendScanLine8( (const char*)bitmap.scanLine(i), width );
    }
}
