/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QPointer>
#include <QColor>
#include <QRect>
#include <QByteArray>

class QTcpSocket;

class RfbSocket
{
  public:
    void open( QTcpSocket* );
    void close();

    void sendEncoding32( qint32 );

    void sendUint32( quint32 );
    quint32 receiveUint32();

    void sendUint16( quint16 );
    quint16 receiveUint16();

    void sendUint8( quint8 );
    quint8 receiveUint8();

    void sendString( const char*, int count );
    int readString( char*, int count );

    void sendPadding( int count );
    void receivePadding( int count );

    void sendScanLine32( const QRgb*, int count );
    void sendScanLine8( const char*, int count );

    void sendByteArray( const QByteArray& );

    void sendPoint32( const QPoint& );
    void sendSize32( const QSize& );

    void sendRect64( const QRect& );
    void sendRect64( const QPoint&, const QSize& );

    QRect readRect64();

    qint64 bytesAvailable() const;
    void flush();

  private:
    void sendBytes( const void*, qint64 count );
    qint64 readBytes( void*, qint64 count );

    QPointer< QTcpSocket > m_tcpSocket;
};
