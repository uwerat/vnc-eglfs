/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <qobject.h>
#include <memory>

class VncServer;
class QTcpSocket;

class VncClient : public QObject
{
    Q_OBJECT

  public:
    explicit VncClient( qintptr fd, VncServer* );
    ~VncClient() override;

    void markDirty( const QRect&, bool fullscreen );
    void updateCursor();

  Q_SIGNALS:
    void disconnected();

  private:
    void processClientData();
    void sendFrameBuffer();
    void scheduleUpdate();

    bool handleSetPixelFormat();
    bool handleSetEncodings();
    bool handleFrameBufferUpdateRequest();
    bool handlePointerEvent();
    bool handleKeyEvent();
    bool handleClientCutText();

  private:
    class PrivateData;
    std::unique_ptr< PrivateData > m_data;
};
