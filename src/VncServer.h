/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qobject.h>
#include <qimage.h>
#include <qvector.h>
#include <qmutex.h>
#include <qpointer.h>

class QWindow;
class QTcpServer;
class VncFrameGrabber;

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

    QSize frameSize() const;
    const VncFrameGrabber* frameGrabber() const;

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

    QPointer< QWindow > m_window;
    QVector< QThread* > m_threads;

    VncFrameGrabber* m_frameGrabber = nullptr;
    VncCursor m_cursor;

    QMetaObject::Connection m_grabConnectionId;
};

