/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "VncServer.h"
#include "VncClient.h"

#include <qtcpserver.h>
#include <qopenglcontext.h>
#include <qopenglfunctions.h>
#include <qwindow.h>
#include <qmutex.h>

#include <qpa/qplatformcursor.h>

namespace
{
    /*
        Often EGLFS is in combination with a touch screen, where you do not
        have a cursor and all we need is a dummy cursor so that we can make
        use of the mouse in the VNC client.

        But when having a cursor, it might be updated by an OpenGl shader,
        - like Qt::WaitCursor, that is rotating constantly.

        We have to find out how to deal with this all, but for the moment
        we simply go with a workaround, that acts like when having
        static cursor images.
     */
    VncCursor createCursor( Qt::CursorShape shape )
    {
        QPlatformCursorImage platformImage( nullptr, nullptr, 0, 0, 0, 0 );
        platformImage.set( shape );

        return { *platformImage.image(), platformImage.hotspot() };
    }

#if 0
    VncCursor createCursor( const QCursor* cursor )
    {
        const auto shape = cursor ? cursor->shape() : Qt::ArrowCursor;

        if ( shape == Qt::BitmapCursor )
            return { cursor->pixmap().toImage(), cursor->hotSpot() };

        return createCursor( shape );
    }
#endif

    class TcpServer final : public QTcpServer
    {
        Q_OBJECT

      public:
        TcpServer( QObject* parent )
            : QTcpServer( parent )
        {
        }

      Q_SIGNALS:
        void connectionRequested( qintptr );

      protected:
        void incomingConnection( qintptr socketDescriptor ) override
        {
            /*
                We do not want to use QTcpServer::nextPendingConnection to avoid
                QTcpSocket being created in the wrong thread
             */
            Q_EMIT connectionRequested( socketDescriptor );
        }
    };
}

VncServer::VncServer( int port, QWindow* window )
    : m_window( window )
    , m_cursor( createCursor( Qt::ArrowCursor ) )
{
    Q_ASSERT( window && window->inherits( "QQuickWindow" ) );

    m_window = window;

    auto tcpServer = new TcpServer( this );
    connect( tcpServer, &TcpServer::connectionRequested, this, &VncServer::addClient );

    if( tcpServer->listen( QHostAddress::Any, port ) )
        qInfo( "VncServer created on port %d", port);
}

VncServer::~VncServer()
{
    qDeleteAll( m_clients );
}

void VncServer::addClient( qintptr fd )
{
    auto client = new VncClient( fd, this );
    connect( client, &VncClient::disconnected, this, &VncServer::removeClient );

    m_clients += client;

    if ( m_window && !m_grabConnectionId )
    {
        /*
            frameSwapped is from the scene graph thread, so we
            need a Qt::DirectConnection to avoid, that the image is
            already gone, when being scheduled from a Qt::QQueuedConnection !
         */

        m_grabConnectionId = QObject::connect( m_window, SIGNAL(frameSwapped()),
            this, SLOT(updateFrameBuffer()), Qt::DirectConnection );
        
        QMetaObject::invokeMethod( m_window, "update" );
    }

    if ( auto tcpServer = qobject_cast< const TcpServer* >( sender() ) )
    {
        qInfo( "New VNC client attached on port %d, #clients: %d",
            tcpServer->serverPort(), m_clients.count() );
    }
}

void VncServer::removeClient()
{
    if ( auto client = qobject_cast< VncClient* >( sender() ) )
    {
        m_clients.removeOne( client );
        client->deleteLater();

        if ( m_clients.isEmpty() && m_grabConnectionId )
            QObject::disconnect( m_grabConnectionId );

        qInfo( "VNC client detached, #clients: %d",  m_clients.count() );
    }
}

void VncServer::updateFrameBuffer()
{
    {
        QMutexLocker locker( &m_frameBufferMutex );

        if ( m_window->size() != m_frameBuffer.size() )
        {
            /*
                On EGLFS the window always matches the screen size.

                But when testing the implementation on X11 the window
                might be resized manually later. Should be no problem,
                as most clients indicate being capable of adjustments
                of the framebuffer size. ( "DesktopSize" pseudo encoding )
             */

            m_frameBuffer = QImage( m_window->size(), QImage::Format_RGB32 );
        }

        QOpenGLContext::currentContext()->functions()->glReadPixels(
            0, 0, m_frameBuffer.width(), m_frameBuffer.height(),
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_frameBuffer.bits() );

        // OpenGL images are vertically flipped.
        m_frameBuffer = std::move( m_frameBuffer ).mirrored( false, true );
    }

    const QRect rect( 0, 0, m_frameBuffer.width(), m_frameBuffer.height() );

    for ( auto client : qAsConst( m_clients ) )
        client->markDirty( rect, true );
}

QWindow* VncServer::window() const
{
    return m_window;
}

QImage VncServer::frameBuffer() const
{
    QMutexLocker locker( &m_frameBufferMutex );
    const auto fb = m_frameBuffer;

    return fb;
}

VncCursor VncServer::cursor() const
{
    return m_cursor;
}

#include "VncServer.moc"
#include "moc_VncServer.cpp"
