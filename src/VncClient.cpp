/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "VncClient.h"
#include "VncServer.h"

#include "RfbSocket.h"
#include "RfbInputEventHandler.h"
#include "RfbPixelStreamer.h"

#include <qtcpsocket.h>

#include <qcoreapplication.h>
#include <qvarlengtharray.h>
#include <qendian.h>
#include <qwindow.h>
#include <qtimer.h>

enum ClientState
{
    Protocol,
    Init,
    Connected
};

class VncClient::PrivateData
{
  public:
    PrivateData( VncServer* server )
        : server( server )
    {
    }

    QWindow* window() const
    {
        return server->window();
    }

    VncServer* server;

    RfbSocket socket;
    RfbPixelStreamer pixelStreamer;

    bool cursorEnabled = false;
    bool screenResizable = false;

    int state = -1;

    int messageType = -1;
    int pendingBytes = 0;

    QSize frameBufferSize;

    // supported encodings in order of preference
    QVector< qint32 > encodings;

    bool frameRequested = false;
    bool frameDirty = true;

    QTimer updateTimer;
};

VncClient::VncClient( qintptr socketDescriptor, VncServer* server )
    : m_data( new PrivateData( server ) )
{
    auto socket = new QTcpSocket( this );
    socket->setSocketDescriptor( socketDescriptor );

    connect( socket, &QTcpSocket::readyRead, this, &VncClient::processClientData );
    connect( socket, &QTcpSocket::disconnected, this, &VncClient::disconnected );
    connect( socket, &QTcpSocket::disconnected, &m_data->updateTimer, &QTimer::stop );

    m_data->socket.open( socket );

    // checking every 30ms for updates
    m_data->updateTimer.setInterval( 30 );
    connect( &m_data->updateTimer, &QTimer::timeout, this, &VncClient::maybeSendFrameBuffer );

    // send protocol version
    const char proto[] = "RFB 003.003\n";
    m_data->socket.sendString( proto, 12 );

    m_data->state = Protocol;
}

VncClient::~VncClient()
{
    m_data->socket.close();
}

void VncClient::markDirty()
{
    m_data->frameDirty = true;
}

void VncClient::processClientData()
{
    if ( m_data->window() == nullptr )
        return;

    auto socket = &m_data->socket;

    if ( m_data->state == Protocol )
    {
        if ( socket->bytesAvailable() >= 12 )
        {
            /*
                protocol version string: "RFB xxx.yyy\n"
                As the client is not allowed to respond with a higher
                version we already know that it is "RFB 003.003\n"
             */
            char version[13];
            socket->readString( version, 12 );
            version[12] = '\0';

            enum Authentification
            {
                None = 1,
                VNC = 2
            };

            socket->sendUint32( None );
            m_data->state = Init;
        }

        return;
    }

    if ( m_data->state == Init )
    {
        if ( socket->bytesAvailable() >= 1 )
        {
            // 6.3.1 Security

            const auto securityType = socket->receiveUint8();
            Q_UNUSED( securityType )

            // 6.3.2 ServerInit

            socket->sendSize32( m_data->window()->size() );
            m_data->pixelStreamer.sendServerFormat( socket );

            const char name[] = "VNC Server for Qt/Quick on EGLFS";
            const auto length = sizeof( name ) - 1;

            socket->sendUint32( length );
            socket->sendString( name, length );

            m_data->state = Connected;
        }

        return;
    }

    if ( m_data->state == Connected )
    {
        enum
        {
            SetPixelFormat = 0,
            FixColourMapEntries = 1,
            SetEncodings = 2,
            FramebufferUpdateRequest = 3,
            KeyEvent = 4,
            PointerEvent = 5,
            ClientCutText = 6
        };

        do
        {
            if ( m_data->messageType < 0 )
                m_data->messageType = socket->receiveUint8();

            bool done = false;

            switch ( m_data->messageType )
            {
                case SetPixelFormat:
                    done = handleSetPixelFormat();
                    break;

                case SetEncodings:
                    done = handleSetEncodings();
                    break;

                case FramebufferUpdateRequest:
                    done = handleFrameBufferUpdateRequest();
                    break;

                case KeyEvent:
                    done = handleKeyEvent();
                    break;

                case PointerEvent:
                    done = handlePointerEvent();
                    break;

                case ClientCutText:
                    done = handleClientCutText();
                    break;

                case FixColourMapEntries:
                    done = true; // ignored
                    break;

                default:
                    qWarning("Unknown message type: %d", (int)m_data->messageType);
                    done = true;
            }

            if ( done )
                m_data->messageType = -1;

        } while ( ( m_data->messageType < 0 ) && socket->bytesAvailable() );
    }
}

void VncClient::maybeSendFrameBuffer()
{
    if ( !( m_data->frameRequested && m_data->frameDirty ) )
    {
        /*
            Better skip this interval to avoid flooding the client
            or hogging the network
         */
        return;
    }

    const auto fb = m_data->server->frameBuffer();

    m_data->frameRequested = m_data->frameDirty = false;

    if ( !fb.isNull() )
    {
        if ( fb.size() != m_data->frameBufferSize )
        {
            if ( m_data->screenResizable )
            {
                auto socket = &m_data->socket;

                socket->sendUint8( 0 ); // msg type
                socket->sendPadding( 1 );
                socket->sendUint16( 1 );
                socket->sendRect64( QPoint(), fb.size() );
                socket->sendEncoding32( -223 );
            }

            m_data->frameBufferSize = fb.size();
        }

        const QRect rect( 0, 0, fb.width(), fb.height() );
        m_data->pixelStreamer.sendImageRaw( fb, rect, &m_data->socket );
    }
}

bool VncClient::handleSetPixelFormat()
{
    auto socket = &m_data->socket;

    if ( socket->bytesAvailable() < 19 )
        return false;

    m_data->pixelStreamer.receiveClientFormat( socket );

    return true;
}

bool VncClient::handleSetEncodings()
{
    auto socket = &m_data->socket;
    auto& count = m_data->pendingBytes;

    if ( count == 0 )
    {
        if ( socket->bytesAvailable() >= 3 )
        {
            socket->receivePadding( 1 );
            count = socket->receiveUint16();
        }
    }

    const auto bytesAvailable = (unsigned) socket->bytesAvailable();
    if ( bytesAvailable < count * sizeof( quint32 ) )
        return false;

    for ( int i = 0; i < count; ++i )
    {
        // see: https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#encodings

        enum Encoding
        {
            Raw = 0,
            CopyRect = 1,
            RRE = 2,
            Hextile = 5,
            ZRLE = 16,

            // pseudo encodings
            Cursor = -239,
            DesktopSize = -223
        };

        const qint32 encoding = socket->receiveUint32();
        m_data->encodings += encoding;

        if ( encoding == -239 )
        {
            // Cursor
            m_data->cursorEnabled = true;
            updateCursor();
        }
        else if ( encoding == -223 )
        {
            // DesktopSize
            m_data->screenResizable = true;
        }
    }

    count = 0;
    return true;
}

bool VncClient::handleFrameBufferUpdateRequest()
{
    auto socket = &m_data->socket;

    if ( socket->bytesAvailable() < 9 )
        return false;

    if ( !m_data->updateTimer.isActive() )
    {
        /*
            To avoid sending frames before knowing the pixel format
            from the client we wait for the initial FramebufferUpdateRequest
         */

        m_data->updateTimer.start();
    }

    m_data->frameRequested = true;

    const bool incremental = socket->receiveUint8();
    (void)socket->readRect64();

    if ( !incremental )
        markDirty();

    return true;
}

bool VncClient::handlePointerEvent()
{
    auto socket = &m_data->socket;

    if ( socket->bytesAvailable() < 5 )
        return false;

    const auto buttonMask = socket->receiveUint8();

    const auto x = socket->receiveUint16();
    const auto y = socket->receiveUint16();

    if ( auto window = m_data->window() )
        Rfb::handlePointerEvent( QPoint( x, y ), buttonMask, window );

    return true;
}

bool VncClient::handleKeyEvent()
{
    auto socket = &m_data->socket;

    if ( socket->bytesAvailable() < 7 )
        return false;

    const auto down = socket->receiveUint8();
    socket->receivePadding( 2 );

    const quint32 key = socket->receiveUint32();

    if ( auto window = m_data->window() )
        Rfb::handleKeyEvent( key, down, window );

    return true;
}

bool VncClient::handleClientCutText()
{
    auto socket = &m_data->socket;
    auto& length = m_data->pendingBytes;

    if ( length == 0 )
    {
        if ( socket->bytesAvailable() < 7 )
            return false;

        socket->receivePadding( 3 );
        length = socket->receiveUint32();
    }

    if ( length )
    {
        if ( socket->bytesAvailable() < length )
            return false;

        // what to do with text ???
        QVarLengthArray< char > text( length + 1 );
        socket->readString( text.data(), length );

        length = 0;
    }

    return true;
}

void VncClient::updateCursor()
{
    if ( m_data->cursorEnabled )
    {
        const auto cursor = m_data->server->cursor();
        m_data->pixelStreamer.sendCursor(
            cursor.hotspot, cursor.image, &m_data->socket );
    }
}

#include "moc_VncClient.cpp"
