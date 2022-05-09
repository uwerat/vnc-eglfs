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

        bool eventFilter( QObject*, QEvent* ) override;
        VncServer* server( const QWindow* ) const;
      
      private:
        int m_port = 5900;
        QVector< VncServer* > m_servers;
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

    VncManager* vncManager = nullptr;
}

static void vncDestroy()
{
    delete vncManager;
}

static void vncInit()
{
    vncManager = new VncManager();
    qAddPostRoutine( vncDestroy );

    qDebug() << "VNC: QQuickWindows are accessible from ports >= 5900";
}

Q_COREAPP_STARTUP_FUNCTION( vncInit )

void Vnc::setup()
{
    // for the moment simply a dummy call so that the shared lib gets loaded
}

VncManager::VncManager()
{
    for ( auto window : QGuiApplication::topLevelWindows() )
    {
        if ( window->isExposed() && isOpenGLQuickWindow( window ) )
            m_servers += new VncServer( m_port++, window );
    }

    QCoreApplication::instance()->installEventFilter( this );
}

VncManager::~VncManager()
{
    qDeleteAll( m_servers );
}

VncServer* VncManager::server( const QWindow* window ) const
{
    if ( window )
    {
        for ( auto server : m_servers )
        {
            if ( server->window() == window )
                return server;
        }
    }

    return nullptr;
}

bool VncManager::eventFilter( QObject* object, QEvent* event )
{
    if ( event->type() == QEvent::Expose )
    {
        auto window = qobject_cast< QWindow* >( object );

        if ( isOpenGLQuickWindow( window ) )
        {
            if ( server( window ) == nullptr )
                m_servers += new VncServer( m_port++, window );
        }
    }
    else if ( event->type() == QEvent::Close )
    {
        // TODO ...
    }

    return QObject::eventFilter( object, event );
}
