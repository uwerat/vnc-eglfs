/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <qobject.h>
#include <memory>

class VncServer;
class QTcpSocket;

class VncClient final : public QObject
{
    Q_OBJECT

  public:
    explicit VncClient( qintptr fd, VncServer* );
    ~VncClient() override;

    void setTimerInterval( int ms );
    int timerInterval() const;

    void markDirty();
    void updateCursor();

  Q_SIGNALS:
    void disconnected();

  private:
    void processClientData();
    void maybeSendFrameBuffer();

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
