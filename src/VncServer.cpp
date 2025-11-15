/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncServer.h"
#include "VncClient.h"

#include <qtcpserver.h>
#include <qopenglcontext.h>
#include <qopenglfunctions.h>
#include <qwindow.h>
#include <qthread.h>
#include <qelapsedtimer.h>
#include <qloggingcategory.h>

#include <qpa/qplatformcursor.h>

#include <QOpenGLExtraFunctions>

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

    auto tcpServer = new TcpServer( this );
    connect( tcpServer, &TcpServer::connectionRequested, this, &VncServer::addClient );

    m_tcpServer = tcpServer;

    if( m_tcpServer->listen( QHostAddress::Any, port ) )
        qCDebug( logConnection ) << "VncServer created on port" << port;
}

VncServer::~VncServer()
{
    m_window = nullptr;

    const auto& threads = m_threads; // qAsConst is deprecated in Qt6.7, std::as_const is C++17
    for ( auto thread : threads )
    {
        thread->quit();
        thread->wait( 20 );
    }

    if ( m_textureId )
    {
        GLuint textureId = m_textureId;
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &textureId);
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

    if ( m_window && !m_grabConnectionId )
    {
        /*
            afterRendering is from the scene graph thread, so we
            need a Qt::DirectConnection to avoid, that the image is
            already gone, when being scheduled from a Qt::QQueuedConnection !
         */

        m_grabConnectionId = QObject::connect( m_window, SIGNAL(afterRendering()),
            this, SLOT(updateFrameBuffer()), Qt::DirectConnection );

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
        if ( m_threads.isEmpty() && m_grabConnectionId )
            QObject::disconnect( m_grabConnectionId );

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

static void fillTexture( const unsigned int textureId, const QSize& size )
{
    const int width = size.width();
    const int height = size.height();

    const auto context = QOpenGLContext::currentContext();

    auto& f = *context->functions();

    f.glBindTexture( GL_TEXTURE_2D, textureId );
    f.glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8,
        size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    f.glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    f.glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

    GLuint fbo;
    f.glGenFramebuffers( 1, &fbo );
    f.glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );

    f.glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0 );

    if ( f.glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
        qDebug() << "FBO setup failed!";

    f.glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
    context->extraFunctions()->glReadBuffer( GL_BACK );

    {
        typedef void ( QOPENGLF_APIENTRYP PFNGLBLITFRAMEBUFFERPROC )(
            GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);

        auto blitFBO = (PFNGLBLITFRAMEBUFFERPROC) context->getProcAddress("glBlitFramebuffer");

        blitFBO( 0, 0, width, height,
            0, height, width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST ); // copy + flip
    }

    f.glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    f.glDeleteFramebuffers( 1, &fbo );
}

static QImage grabTexture( const unsigned int textureId, const QSize& size )
{
    QImage image( size, QImage::Format_RGB32 );

    auto& f = *QOpenGLContext::currentContext()->functions();

    f.glBindTexture(GL_TEXTURE_2D, textureId );

    const GLenum glFormat = GL_BGRA;
#if 0
    glGetTexImage( GL_TEXTURE_2D, 0, glFormat, GL_UNSIGNED_BYTE, image.bits() );
#else

    // Create a temporary framebuffer
    GLuint fbo = 0;
    f.glGenFramebuffers(1, &fbo);
    f.glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Attach the texture
    f.glFramebufferTexture2D(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);

    // Check FBO completeness
    GLenum status = f.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if ( status != GL_FRAMEBUFFER_COMPLETE )
        qDebug() << "FBO setup failed!";

    // Read pixels from the framebuffer
    f.glReadPixels(0, 0, size.width(), size.height(),
        glFormat, GL_UNSIGNED_BYTE, image.bits() );

    // Clean up framebuffer
    f.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    f.glDeleteFramebuffers(1, &fbo);
#endif

    return image;
}

void VncServer::updateFrameBuffer()
{
    const auto size = m_window->size() * m_window->devicePixelRatio();

    {
        QMutexLocker locker( &m_frameBufferMutex );

        if ( m_textureId == 0 )
        {
            auto context = QOpenGLContext::currentContext();
            context->extraFunctions()->initializeOpenGLFunctions();

            GLuint textureId;
            context->functions()->glGenTextures( 1, &textureId );
            m_textureId = textureId;
        }

        fillTexture( m_textureId, size );
    }

    m_frameBuffer = grabTexture( m_textureId, size );

    const QRect rect( 0, 0, size.width(), size.height() );

    const auto& threads = m_threads;
    for ( auto thread : threads )
    {
        auto clientThread = static_cast< ClientThread* >( thread );
        clientThread->markDirty();
    }
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
