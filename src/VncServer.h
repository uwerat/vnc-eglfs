/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <qobject.h>
#include <qimage.h>
#include <qvector.h>
#include <qmutex.h>
#include <qpointer.h>

class QWindow;
class QTcpServer;

class VncCursor
{
  public:
    QImage image;
    QPoint hotspot;
};

class VncServer final : public QObject
{
    Q_OBJECT

  public:
    VncServer( int port, QWindow* );
    ~VncServer() override;

    void setName( const QString& name )
    {
        m_name = name;
    }
    QString name() const
    {
        return m_name;
    }

    QImage frameBuffer() const;
    VncCursor cursor() const;

    QWindow* window() const;
    int port() const;

    void setTimerInterval( int ms );

  private Q_SLOTS:
    void updateFrameBuffer();

  private:
    void addClient( qintptr fd );
    void removeClient();

    QTcpServer* m_tcpServer = nullptr;
    QString m_name{ "VNC Server for Qt/Quick on EGLFS" };

    QPointer< QWindow > m_window;
    QVector< QThread* > m_threads;

    mutable QMutex m_frameBufferMutex;
    QImage m_frameBuffer;

    VncCursor m_cursor;

    QMetaObject::Connection m_grabConnectionId;
};

