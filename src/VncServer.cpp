/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncServer.h"
#include "VncClient.h"
#include "VncFrame.h"
#include "VncFrameGrabber.h"

#include <qtcpserver.h>
#include <qopenglcontext.h>
#include <qwindow.h>
#include <qthread.h>
#include <qelapsedtimer.h>
#include <qloggingcategory.h>
#include <qreadwritelock.h>
#include <qvector.h>
#include <qpointer.h>
#include <qimage.h>
#include <qbuffer.h>
#include <qimagewriter.h>

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
}

namespace
{
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

namespace
{
    QByteArray frameToJPEG( const VncFrame& frame, int quality )
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

    VncFrame clippedFrame( const VncFrame& frame, const QRect& region )
    {
        if ( region == frame.region() )
            return frame;

        const auto stride = region.width() * sizeof( QRgb );

        QByteArray bytes( region.height() * stride, Qt::Uninitialized );

        auto from = reinterpret_cast< const QRgb* >( frame.bytes() );
        from += region.top() * frame.width() + region.left();

        auto to = reinterpret_cast< QRgb* >( bytes.data() );

        for ( int i = 0; i < region.height(); i++ )
        {
            memcpy( to, from, stride );

            from += frame.width();
            to += region.width();
        }

        return VncFrame( VncFrame::Rgb, region, bytes );
    }
}

namespace
{
    class FrameCache
    {
      public:
        void clear() { m_jpegFrames.clear(); }

        VncFrame& frame( int hash = 0 ) { return m_jpegFrames[ hash ]; }
        VncFrame& frame( const QRect& rect, int quality )
            { return frame( qHash( rect, quality ) ); }

      private:
        QHash< int, VncFrame > m_jpegFrames;
    };
}

class VncServer::PrivateData
{
  public:
    QTcpServer* tcpServer = nullptr;

    QPointer< QWindow > window;
    QVector< QThread* > threads;

    VncFrameGrabber* grabber = nullptr;
    mutable FrameCache cache;

    VncCursor cursor;

    QMetaObject::Connection connections[2];
    QReadWriteLock lock;
};

VncServer::VncServer( int port, QWindow* window )
    : m_data( new PrivateData() )
{
    Q_ASSERT( window && window->inherits( "QQuickWindow" ) );

    m_data->window = window;
    m_data->cursor = createCursor( Qt::ArrowCursor );

    auto tcpServer = new TcpServer( this );
    connect( tcpServer, &TcpServer::connectionRequested, this, &VncServer::addClient );

    m_data->tcpServer = tcpServer;

    if( tcpServer->listen( QHostAddress::Any, port ) )
        qCDebug( logConnection ) << "VncServer created on port" << port;
}

VncServer::~VncServer()
{
    m_data->window = nullptr;

    const auto& threads = m_data->threads;
    for ( auto thread : threads )
    {
        thread->quit();
        thread->wait( 20 );
    }

    delete m_data->grabber;
}

int VncServer::port() const
{
    return m_data->tcpServer->serverPort();
}

void VncServer::addClient( qintptr fd )
{
    auto thread = new ClientThread( fd, this );
    m_data->threads += thread;

    auto& connections = m_data->connections;
    auto window = m_data->window.data();

    if ( window && !connections[0] )
    {
        /*
            Qt::DirectConnection: we want to execute the slots on
            the scene graph thread
         */

        connections[0] = QObject::connect( window, SIGNAL(afterRendering()),
            this, SLOT(copyWindowBuffer()), Qt::DirectConnection );

        connections[1] = QObject::connect( window, SIGNAL(sceneGraphInvalidated()),
            this, SLOT(pauseServer()), Qt::DirectConnection );

        QMetaObject::invokeMethod( window, "update" );
    }

    qCDebug( logConnection ) << "New VNC client attached on port"
        << m_data->tcpServer->serverPort() << "#clients" << m_data->threads.count();

    connect( thread, &QThread::finished, this, &VncServer::removeClient );
    thread->start();
}

void VncServer::removeClient()
{
    auto thread = qobject_cast< QThread* >( sender() );
    if ( thread == nullptr )
        return;

    auto& threads = m_data->threads;
    auto& connections = m_data->connections;

    threads.removeOne( thread );

    if ( threads.isEmpty() && connections[0] )
    {
        if ( connections[0] )
        {
            QObject::disconnect( connections[0] );
            QObject::disconnect( connections[1] );
        }

        pauseServer();
    }

    thread->quit();
    thread->wait( 100 );

    delete thread;

    qCDebug( logConnection )
        << "VNC client detached on port" << m_data->tcpServer->serverPort()
        << "#clients:" << threads.count();
}

void VncServer::setTimerInterval( int ms )
{
    const auto& threads = m_data->threads;
    for ( auto thread : threads )
    {
        auto client = static_cast< ClientThread* >( thread )->client();
        client->setTimerInterval( ms );
    }
}

QSize VncServer::windowBufferSize() const
{
    const auto* window = m_data->window.data();
    return window->size() * window->devicePixelRatio();
}

void VncServer::copyWindowBuffer()
{
    QWriteLocker locker( &m_data->lock );

    if ( m_data->grabber == nullptr )
        m_data->grabber = new VncFrameGrabber();

    m_data->grabber->importBackBuffer( windowBufferSize() );
    m_data->cache.clear();

    const auto& threads = m_data->threads;
    for ( auto thread : threads )
    {
        auto clientThread = static_cast< ClientThread* >( thread );
        clientThread->markDirty();
    }
}

void VncServer::pauseServer()
{
    QWriteLocker locker( &m_data->lock );

    delete m_data->grabber;
    m_data->grabber = nullptr;

    m_data->cache.clear();
}

VncFrame VncServer::grabFrame( const QRect& region, int qualityLevel ) const
{
    auto grabber = m_data->grabber;
    if ( grabber == nullptr )
        return VncFrame();

    auto& cache = m_data->cache;

    if ( qualityLevel == 0 )
    {
        auto& frame = cache.frame();
        if ( frame.byteCount() == 0 )
            frame = grabber->grabFrame( region, 0 );

        return clippedFrame( frame, region );
    }

    /*
        quality: [1:100], level: [0,9].
        Higher means better quality + less compression
     */

    const auto quality = ( qualityLevel + 1 ) * 10;

    auto& frame = cache.frame( region, quality );

    if ( frame.byteCount() == 0 )
    {
        if ( grabber->supportsVideoAcceleration() )
        {
            frame = grabber->grabFrame( region, quality );
        }
        else
        {
            const auto frm = grabFrame( region, 0 );

            frame.setFrame( VncFrame::Jpeg,
                region, frameToJPEG( frm, quality ) );
        }
    }

    return frame;
}

QWindow* VncServer::window() const
{
    return m_data->window;
}

QReadWriteLock* VncServer::lock() const
{
    return &m_data->lock;
}

VncCursor VncServer::cursor() const
{
    return m_data->cursor;
}

#include "VncServer.moc"
#include "moc_VncServer.cpp"
