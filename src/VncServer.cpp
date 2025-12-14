/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncServer.h"
#include "VncClient.h"
#include "VncFrameGrabber.h"

#include <qtcpserver.h>
#include <qopenglcontext.h>
#include <qwindow.h>
#include <qthread.h>
#include <qelapsedtimer.h>
#include <qloggingcategory.h>
#include <qreadwritelock.h>

#include <qpa/qplatformcursor.h>

Q_LOGGING_CATEGORY( logGrab, "vnceglfs.grab", QtCriticalMsg )
Q_LOGGING_CATEGORY( logConnection, "vnceglfs.connection" )

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

    class ClientThread : public QThread
    {
      public:
        ClientThread( qintptr socketDescriptor, VncServer* server )
            : QThread( server )
            , m_socketDescriptor( socketDescriptor )
        {
        }

        ~ClientThread()
        {
        }

        void markDirty()
        {
            if ( m_client )
                m_client->markDirty();
        }

        VncClient* client() const { return m_client; }

      protected:
        void run() override
        {
            VncClient client( m_socketDescriptor, qobject_cast< VncServer* >( parent() ) );
            connect( &client, &VncClient::disconnected, this, &QThread::quit );

            m_client = &client;
            QThread::run();
            m_client = nullptr;
        }

      private:
        VncClient* m_client = nullptr;
        const qintptr m_socketDescriptor;
    };
}

VncServer::VncServer( int port, QWindow* window )
    : m_window( window )
    , m_cursor( createCursor( Qt::ArrowCursor ) )
{
    Q_ASSERT( window && window->inherits( "QQuickWindow" ) );

    m_window = window;
    m_frameGrabber = new VncFrameGrabber( this );

    auto tcpServer = new TcpServer( this );
    connect( tcpServer, &TcpServer::connectionRequested, this, &VncServer::addClient );

    m_tcpServer = tcpServer;

    if( m_tcpServer->listen( QHostAddress::Any, port ) )
        qCDebug( logConnection ) << "VncServer created on port" << port;
}

VncServer::~VncServer()
{
    delete m_frameGrabber;
    m_window = nullptr;

    const auto& threads = m_threads; // qAsConst is deprecated in Qt6.7, std::as_const is C++17
    for ( auto thread : threads )
    {
        thread->quit();
        thread->wait( 20 );
    }
}

int VncServer::port() const
{
    return m_tcpServer->serverPort();
}

void VncServer::addClient( qintptr fd )
{
    auto thread = new ClientThread( fd, this );
    m_threads += thread;

    if ( m_window && !m_connections[0] )
    {
        /*
            Qt::DirectConnection: we want to execute the slots on
            the scene graph thread
         */

        m_connections[0] = QObject::connect( m_window, SIGNAL(afterRendering()),
            this, SLOT(updateFrame()), Qt::DirectConnection );

        m_connections[1] = QObject::connect( m_window, SIGNAL(sceneGraphInvalidated()),
            this, SLOT(invalidateFrame()), Qt::DirectConnection );

        QMetaObject::invokeMethod( m_window, "update" );
    }

    qCDebug( logConnection ) << "New VNC client attached on port" << m_tcpServer->serverPort()
        << "#clients" << m_threads.count();

    connect( thread, &QThread::finished, this, &VncServer::removeClient );
    thread->start();
}

void VncServer::removeClient()
{
    if ( auto thread = qobject_cast< QThread* >( sender() ) )
    {
        m_threads.removeOne( thread );
        if ( m_threads.isEmpty() && m_connections[0] )
        {
            if ( m_connections[0] )
            {
                QObject::disconnect( m_connections[0] );
                QObject::disconnect( m_connections[1] );
            }

            invalidateFrame();
        }

        thread->quit();
        thread->wait( 100 );

        delete thread;

        qCDebug( logConnection ) << "VNC client detached on port" << m_tcpServer->serverPort()
            << "#clients:" << m_threads.count();
    }
}

void VncServer::setTimerInterval( int ms )
{
    const auto& threads = m_threads;
    for ( auto thread : threads )
    {
        auto client = static_cast< ClientThread* >( thread )->client();
        client->setTimerInterval( ms );
    }
}

void VncServer::updateFrame()
{
    QWriteLocker locker( &m_lock );

    const auto sz = m_window->size() * m_window->devicePixelRatio();
    m_frameGrabber->update( sz );

    const auto& threads = m_threads;
    for ( auto thread : threads )
    {
        auto clientThread = static_cast< ClientThread* >( thread );
        clientThread->markDirty();
    }
}

void VncServer::invalidateFrame()
{
    QWriteLocker locker( &m_lock );
    m_frameGrabber->invalidate();
}

QWindow* VncServer::window() const
{
    return m_window;
}

const VncFrameGrabber* VncServer::frameGrabber() const
{
    return m_frameGrabber;
}

QReadWriteLock* VncServer::lock() const
{
    return &m_lock;
}

VncCursor VncServer::cursor() const
{
    return m_cursor;
}

#include "VncServer.moc"
#include "moc_VncServer.cpp"
