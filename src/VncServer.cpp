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

    void grabPixels( QImage& frameBuffer )
    {
        auto context = QOpenGLContext::currentContext();

        context->functions()->glReadPixels(
            0, 0, frameBuffer.width(), frameBuffer.height(),
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, frameBuffer.bits() );

        // OpenGL images are vertically flipped.
#if 1
        /*
            Actually we do not need to flip the image here. We could
            simply iterate in inverted order, when sending the pixels
            in RfbPixelStreamer. TODO ...
         */
        frameBuffer = frameBuffer.mirrored( false, true );
#endif
    }

    void resizeFrameBuffer( const QSize& size, QImage& frameBuffer )
    {
        if ( size != frameBuffer.size() )
        {
            /*
                On EGLFS the window always matches the screen size.

                But when testing the implementation on X11 the window
                might be resized manually later. Should be no problem,
                as most clients indicate being capable of adjustments
                of the framebuffer size. ( "DesktopSize" pseudo encoding )
             */

            frameBuffer = QImage( size, QImage::Format_RGB32 );
        }
    }

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

    /*
        frameSwapped is from the scene graph thread, so we
        need a Qt::DirectConnection to avoid, that the image is
        already gone, when being scheduled from a Qt::QQueuedConnection !
     */
    QObject::connect( window, SIGNAL(frameSwapped()),
        this, SLOT(updateFrameBuffer()), Qt::DirectConnection );
    
    auto tcpServer = new TcpServer( this );
    connect( tcpServer, &TcpServer::connectionRequested, this, &VncServer::addClient );

    if( tcpServer->listen( QHostAddress::Any, port ) )
        qWarning("VncServer created on port %d", port);
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

    // socket->localPort() ...
    qDebug() << "New VNC client attached: " << m_clients.count();
}

void VncServer::removeClient()
{
    if ( auto client = qobject_cast< VncClient* >( sender() ) )
    {
        m_clients.removeOne( client );
        client->deleteLater();

        qDebug() << "VNC client detached: " << m_clients.count();
    }
}

void VncServer::updateFrameBuffer()
{
    resizeFrameBuffer( m_window->size(), m_frameBuffer );
    grabPixels( m_frameBuffer );

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
    resizeFrameBuffer( m_window->size(),
         const_cast< VncServer* >( this )->m_frameBuffer );

    return m_frameBuffer;
}

VncCursor VncServer::cursor() const
{
    return m_cursor;
}

#include "VncServer.moc"
#include "moc_VncServer.cpp"
