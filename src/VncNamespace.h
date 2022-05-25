/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#ifndef VNC_NAMESPACE
#define VNC_NAMESPACE

#include <qglobal.h>
#include <qlist.h>

class QWindow;

#ifdef VNC_DLL
    #if defined( VNC_MAKEDLL )     // create a DLL library
        #define VNC_EXPORT Q_DECL_EXPORT
    #else                          // use a DLL library
        #define VNC_EXPORT Q_DECL_IMPORT
    #endif
#endif

#ifndef VNC_EXPORT
    #define VNC_EXPORT
#endif

namespace Vnc
{
    /*!
        \brief Set the initial port

        When no specific port is requested the first unused
        port >= initial port will be used when starting a server.

        \param port Port number
        \sa initialPort(), serverPort(), startServer()
     */
    VNC_EXPORT void setInitialPort( int port );

    /*!
        \brief Initial port

        When no specific port is requested the first unused
        port >= initial port will be used when starting a server.

        The default value can be initialized by the environment variable
        VNC_PORT. If VNC_PORT is not set the default value is 5900.

        \sa setInitialPort()
        \return initial port
     */
    VNC_EXPORT int initialPort();

    /*!
        \brief Enable the autoStart mode

        When autoStart is enabled VNC servers will be started
        for all exposed QQuickWindow - or whenever a new QQuickWindow
        gets exposed.

        Otherwise a server must be started manually by startServer()

        \param on true/false
        \sa isAutoStartEnabled(), initialPort(), startServer()
     */
    VNC_EXPORT void setAutoStartEnabled( bool on );

    /*!
        \return True, when the autoStart mode is enabled
        \sa setAutoStartEnabled()
     */
    VNC_EXPORT bool isAutoStartEnabled();

    /*!
        \brief Start a VNC server for a QQuickWindow

        When the port is < 0 the next free port >= initialPort() will be used

        \param window Window to be mirrored by the server
        \param port Port

        \sa setAutoStartEnabled(), serverPort(), stopServer()
        \note window has to be a QQuickWindow
     */
    VNC_EXPORT bool startServer( QWindow* window, int port = -1 );

    /*!
        \brief Stop the VNC server for a QQuickWindow

        \param window Window mirrored by a server
        \sa setAutoStartEnabled(), startServer()
     */
    VNC_EXPORT bool stopServer( const QWindow* window );

    /*!
        \return Port used by the VNC server of window
        \sa startServer(), setInitialPort()
     */
    VNC_EXPORT int serverPort( const QWindow* );

    /*!
        \return List of all windows, where a VNC server is running
     */
    VNC_EXPORT QList< QWindow* > windows();
}

#endif
