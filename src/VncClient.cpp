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
#include <qmetaobject.h>

#include <qloggingcategory.h>

Q_LOGGING_CATEGORY( logRfb, "vnceglfs.rfb", QtCriticalMsg )
Q_LOGGING_CATEGORY( logFb, "vnceglfs.fb", QtCriticalMsg )

// export QT_LOGGING_RULES="vnceglfs.rfb.debug=true"

namespace Rfb
{
    Q_NAMESPACE

    enum ClientState
    {
        Protocol,
        Init,
        Connected
    };
    Q_ENUM_NS( ClientState )

    enum Encoding : qint64
    {
        // https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#encodings

        Raw      = 0,
        CopyRect = 1,
        RRE      = 2,
        CoRRE    = 4,
        Hextile  = 5,
        Zlib     = 6,
        Tight    = 7,
        ZlibHex  = 8,
        ZRLE     = 16,
        JPEG     = 21,
        OpenH264 = 50,

        TightPNG = -260,

        DesktopSize = -223,
        LastRect    = -224,
        Cursor      = -239,
        XCursor     = -240,

        QEMUPointerMotionChange = -257,
        QEMUExtendedKeyEvent    = -258,
        QEMUAudio               = -259,
        QEMULEDState            = -261,

        GII                 = -305,
        DesktopName         = -307,
        ExtendedDesktopSize = -308,
        XVP                 = -309,
        Fence               = -312,
        ContinuousUpdates   = -313,
        CursorWithAlpha     = -314,

        VMwareCursor              = 0x574d5664,
        VMwareCursorState         = 0x574d5665,
        VMwareCursorPosition      = 0x574d5666,
        VMwareKeyRepeat           = 0x574d5667,
        VMwareLEDState            = 0x574d5668,
        VMwareDisplayModeChange   = 0x574d5669,
        VMwareVirtualMachineState = 0x574d566a,

        ExtendedClipboard = 0xc0a1e5ce,

        Ultra         = 9,
        Ultra2        = 10,
        TRLE          = 15,
        HitachiZYWRLE = 17,
        H264          = 20,
        JRLE          = 22,
    };
    Q_ENUM_NS( Encoding )
}

static inline QDebug operator<<( QDebug debug, const QVector< qint32 >& encodings )
{
    const auto mo = &Rfb::staticMetaObject;
    const auto enumerator = mo->enumerator( mo->indexOfEnumerator( "Encoding" ) );

    debug.nospace();

    debug << "Encodings:";
    for ( const auto encoding : encodings )
    {
        debug << "\n  ";

        if ( encoding >= -512 && encoding <= -412 )
        {
            debug << "JPEGQuality: " << encoding;
        }
        else if ( encoding >= -32 && encoding <= -23 )
        {
            debug << "JPEGQualityLevel: " << encoding + 32;
        }
        else if ( encoding >= -768 && encoding <= -763 )
        {
            debug << "JPEGSubsamplingLevel: " << encoding;
        }
        else if ( encoding >= -256 && encoding <= -247 )
        {
            debug << "CompressionLevel: " << encoding + 256;
        }
        else if ( encoding >= 1024 && encoding <= 1099 )
        {
            debug << "RealVNC: " << encoding;
        }
        else if ( ( encoding >= -218 && encoding <= -33 )
            || ( encoding >= -22 && encoding <= -1 ) )
        {
            debug << "TightOption: " << encoding;
        }
        else
        {
            if ( auto key = enumerator.valueToKey( encoding ) )
            {
                debug << key;
            }
            else
            {
                const auto hex = QString("0x") + QString::number( encoding, 16 );
                debug << encoding << " ( " << hex << " )";
            }
        }
    }
    debug << "\n";
    debug.space();

    return debug;
}

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

    bool tightEnabled = false;
    int jpegLevel = -1;

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

    m_data->state = Rfb::Protocol;
    qCDebug( logRfb ) << "State" << m_data->state;
}

VncClient::~VncClient()
{
    m_data->socket.close();
}

void VncClient::setTimerInterval( int ms )
{
    m_data->updateTimer.setInterval( ms );
}

int VncClient::timerInterval() const
{
    return m_data->updateTimer.interval();
}

void VncClient::markDirty()
{
    if ( m_data->frameDirty == false )
    {
        qCDebug( logFb ) << "FB dirty";
        m_data->frameDirty = true;
    }
}

void VncClient::processClientData()
{
    if ( m_data->window() == nullptr )
        return;

    auto socket = &m_data->socket;

    if ( m_data->state == Rfb::Protocol )
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
            m_data->state = Rfb::Init;

            qCDebug( logRfb ) << "State" << m_data->state;
        }

        return;
    }

    if ( m_data->state == Rfb::Init )
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

            m_data->state = Rfb::Connected;

            qCDebug( logRfb ) << "State" << m_data->state;
        }

        return;
    }

    if ( m_data->state == Rfb::Connected )
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
    const auto fb = m_data->server->frameBuffer();
    if ( fb.isNull() )
        return;

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

    if ( !( m_data->frameRequested && m_data->frameDirty ) )
    {
        /*
            Better skip this interval to avoid flooding the client
            or hogging the network
         */
        return;
    }

    m_data->frameRequested = m_data->frameDirty = false;

    const QRect rect( 0, 0, fb.width(), fb.height() );

    auto& streamer = m_data->pixelStreamer;

    if ( m_data->tightEnabled && m_data->jpegLevel >= 0 )
    {
        streamer.sendImageJPEG( fb, rect, m_data->jpegLevel, &m_data->socket );
    }
    else
    {
        streamer.sendImageRaw( fb, rect, &m_data->socket );
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

        m_data->encodings.clear();
        m_data->tightEnabled = false;
        m_data->cursorEnabled = false;
        m_data->screenResizable = false;
        m_data->jpegLevel = -1;
    }

    const auto bytesAvailable = (unsigned) socket->bytesAvailable();
    if ( bytesAvailable < count * sizeof( quint32 ) )
        return false;

    for ( int i = 0; i < count; ++i )
    {
        const qint32 encoding = socket->receiveUint32();
        m_data->encodings += encoding;

        if ( encoding == Rfb::Tight )
        {
            m_data->tightEnabled = true;
        }
        else if ( encoding == Rfb::Cursor )
        {
            m_data->cursorEnabled = true;
            updateCursor();
        }
        else if ( encoding == Rfb::DesktopSize )
        {
            m_data->screenResizable = true;
        }
        else if ( encoding >= -32 && encoding <= -23 )
        {
            m_data->jpegLevel = 32 + encoding;
        }
        else if ( encoding >= -512 && encoding <= -412 )
        {
            // TODO ...
        }
    }

    qCDebug( logRfb ) << "Encodings\n" << m_data->encodings;

    m_data->pendingBytes = 0;
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

    const bool incremental = socket->receiveUint8();
    (void)socket->readRect64();

    if ( m_data->frameRequested == false )
    {
        m_data->frameRequested = true;
        qCDebug( logFb ) << "FB requested, incremental:" << incremental;
    }

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

#include "VncClient.moc"
#include "moc_VncClient.cpp"
