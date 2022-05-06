/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "RfbSocket.h"
#include <qtcpsocket.h>
#include <qrect.h>

#include <arpa/inet.h>

void RfbSocket::open( QTcpSocket* tcpSocket )
{
    m_tcpSocket = tcpSocket;
}

void RfbSocket::close()
{
    delete m_tcpSocket;
}

void RfbSocket::flush()
{
    if ( m_tcpSocket && m_tcpSocket->isOpen() )
        m_tcpSocket->flush();
}

qint64 RfbSocket::bytesAvailable() const
{
    return m_tcpSocket ? m_tcpSocket->bytesAvailable() : 0;
}

void RfbSocket::sendBytes( const void* data, qint64 count )
{
    if ( m_tcpSocket && m_tcpSocket->isOpen() )
        m_tcpSocket->write( reinterpret_cast< const char* >( data ), count );
}

qint64 RfbSocket::readBytes( void* data, qint64 count )
{
    if ( m_tcpSocket )
        return m_tcpSocket->read( reinterpret_cast< char* >( data ), count );

    return -1;
}

void RfbSocket::sendEncoding32( qint32 value )
{
    sendUint32( value );
}

void RfbSocket::sendUint32( quint32 value )
{
    value = htonl( value );
    sendBytes( &value, sizeof( quint32 ) );
}

void RfbSocket::sendUint16( quint16 value )
{
    value = htons( value );
    sendBytes( &value, sizeof( quint16 ) );
}

void RfbSocket::sendUint8( quint8 value )
{
    sendBytes( &value, sizeof( quint8 ) );
}

quint32 RfbSocket::receiveUint32()
{
    quint32 value;
    readBytes( &value, sizeof( quint32 ) );

    return ntohl(value);
}

quint16 RfbSocket::receiveUint16()
{
    quint16 value;
    readBytes( &value, sizeof( quint16 ) );

    return ntohs( value );
}

quint8 RfbSocket::receiveUint8()
{
    quint8 value;
    readBytes( &value, sizeof( quint8 ) );

    return value;
}

void RfbSocket::sendPoint32( const QPoint& pos )
{
    sendUint16( pos.x() );
    sendUint16( pos.y() );
}

void RfbSocket::sendSize32( const QSize& size )
{
    sendUint16( qMax( size.width(), 0 ) );
    sendUint16( qMax( size.height(), 0 ) );
}

void RfbSocket::sendRect64( const QPoint& pos, const QSize& size )
{
    sendPoint32( pos );
    sendSize32( size );
}

void RfbSocket::sendRect64( const QRect& rect )
{
    sendPoint32( rect.topLeft() );
    sendSize32( rect.size() );
}

QRect RfbSocket::readRect64()
{
    const auto x = receiveUint16();
    const auto y = receiveUint16();
    const auto w = receiveUint16();
    const auto h = receiveUint16();

    return QRect( x, y, w, h);
}

void RfbSocket::sendString( const char* string, int count )
{
    sendBytes( string, count );
}

int RfbSocket::readString( char* data, int count )
{
    return readBytes( data, count );
}

void RfbSocket::sendPadding( int count )
{
    for ( int i = 0; i < count; i++ )
        sendUint8( 0 );
}

void RfbSocket::receivePadding( int count )
{
    for ( int i = 0; i < count; i++ )
        (void )receiveUint8();
}

void RfbSocket::sendScanLine32( const QRgb* line, int count )
{
    sendBytes( line, count * sizeof( QRgb ) );
}

void RfbSocket::sendScanLine8( const char* line, int count )
{
    sendBytes( line, count );
}

void RfbSocket::sendByteArray( const QByteArray& data )
{
    sendBytes( data.constData(), data.size() );
}
