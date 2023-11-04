/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "VncNamespace.h"
#include "VncServer.h"

#include <qguiapplication.h>
#include <qwindow.h>
#include <qdebug.h>

namespace
{
    class VncManager : public QObject
    {
      public:
        VncManager();
        ~VncManager();

        void setAutoStartEnabled( bool );
        bool isAutoStartEnabled() const;

        void setTimerInterval( int ms );
        int timerInterval() const;

        void setInitialPort( int );
        int initialPort() const;

        bool startServer( QWindow*, int port );
        void stopServer( const QWindow* );

        VncServer* server( const QWindow* ) const;
        int serverPort( const QWindow* ) const;

        QList< QWindow* > windows() const;

      private:
        bool eventFilter( QObject*, QEvent* ) override;

        int nextPort() const;
        bool isPortUsed( int port ) const;

        bool m_autoStart = false;
        int m_timerInterval = 30;

        int m_port = -1;
        QVector< VncServer* > m_servers; // usually <= 1
    };

    inline bool isOpenGLQuickWindow( const QObject* object )
    {
        // no qobject_cast to avoid dependencies to Qt/Quick classes
        if ( object && object->inherits( "QQuickWindow" ) )
        {
            auto window = qobject_cast< const QWindow* >( object );
            return window->supportsOpenGL();
        }

        return false;
    }
}

VncManager::VncManager()
{
    bool ok;

    m_port = qEnvironmentVariableIntValue( "QVNC_GL_PORT", &ok );
    if ( !ok )
        m_port = 5900; // default port for VNC

    m_timerInterval = qEnvironmentVariableIntValue( "QVNC_GL_TIMER_INTERVAL", &ok );
    if ( !ok )
        m_timerInterval = 30;

    m_timerInterval = qMax( m_timerInterval, 10 );
}

VncManager::~VncManager()
{
    qDeleteAll( m_servers );
}

bool VncManager::startServer( QWindow* window, int port )
{
    if ( !isOpenGLQuickWindow( window ) )
        return false;

    for ( const auto server : m_servers )
    {
        if ( server->window() == window || server->port() == port )
            return false;
    }

    if ( port < 0 )
        port = nextPort();

    m_servers += new VncServer( port, window );
    return true;
}

void VncManager::stopServer( const QWindow* window )
{
    for ( auto server : m_servers )
    {
        if ( server->window() == window )
        {
            m_servers.removeOne( server );
            delete server;
        }
    }
}

VncServer* VncManager::server( const QWindow* window ) const
{
    for ( auto server : m_servers )
    {
        if ( server->window() == window )
            return server;
    }

    return nullptr;
}

int VncManager::serverPort( const QWindow* window ) const
{
    if ( auto srv = server( window ) )
        return srv->port();

    return -1;
}

QList< QWindow* > VncManager::windows() const
{
    QList< QWindow* > windows;

    for ( auto server : m_servers )
        windows += server->window();

    return windows;
}

bool VncManager::isPortUsed( int port ) const
{
    for ( const auto server : m_servers )
    {
        if ( port == server->port() )
            return true;
    }

    return false;
}

int VncManager::nextPort() const
{
    auto port = initialPort();

    while( isPortUsed( port ) )
        port++;

    return port;
}

bool VncManager::eventFilter( QObject* object, QEvent* event )
{
    if ( event->type() == QEvent::Expose )
    {
        if ( auto window = qobject_cast< QWindow* >( object ) )
            startServer( window, -1 );
    }
    else if ( event->type() == QEvent::Close )
    {
        // TODO ...
    }

    return QObject::eventFilter( object, event );
}

void VncManager::setTimerInterval( int ms )
{
    ms = qMax( ms, 10 );
    if ( ms != m_timerInterval )
    {
        m_timerInterval = ms;
        for ( auto server : m_servers )
            server->setTimerInterval( ms );
    }
}

int VncManager::timerInterval() const
{
    return m_timerInterval;
}

void VncManager::setInitialPort( int port )
{
    if ( port >= 0 )
        m_port = port;
}

int VncManager::initialPort() const
{
    return m_port;
}

void VncManager::setAutoStartEnabled( bool on )
{
    if ( on == m_autoStart )
        return;

    auto app = QCoreApplication::instance();

    if ( on )
    {
        for ( auto window : QGuiApplication::topLevelWindows() )
        {
            if ( window->isExposed() )
                startServer( window, -1 );
        }

        app->installEventFilter( this );
    }
    else
    {
        app->removeEventFilter( this );
    }
}

bool VncManager::isAutoStartEnabled() const
{
    return m_autoStart;
}

Q_GLOBAL_STATIC( VncManager, vncManager )

namespace Vnc
{
    void setTimerInterval( int ms ) { vncManager->setTimerInterval( ms ); }
    int timerInterval() { return vncManager->timerInterval(); }

    void setInitialPort( int port ) { vncManager->setInitialPort( port ); }
    int initialPort() { return vncManager->initialPort(); }

    void setAutoStartEnabled( bool on ) { vncManager->setAutoStartEnabled( on ); }
    bool isAutoStartEnabled() { return vncManager->isAutoStartEnabled(); }

    bool startServer( QWindow* w, int port ) { return vncManager->startServer( w, port ); }
    void stopServer( const QWindow* w ) { vncManager->stopServer( w ); }

    QList< QWindow* > windows() { return vncManager->windows(); }
    int serverPort( const QWindow* w ) { return vncManager->serverPort( w ); }
}
