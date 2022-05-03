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

class VncClient;
class QWindow;

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

    QImage frameBuffer() const;
    VncCursor cursor() const;

    QWindow* window() const;

  private Q_SLOTS:
    void updateFrameBuffer();

  private:
    void addClient( qintptr fd );
    void removeClient();

    QPointer< QWindow > m_window;
    QVector< VncClient* > m_clients;

    mutable QMutex m_frameBufferMutex;
    QImage m_frameBuffer;

    VncCursor m_cursor;

    QMetaObject::Connection m_grabConnectionId;
};

