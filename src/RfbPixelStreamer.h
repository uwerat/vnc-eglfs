/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <memory>
#include <qglobal.h>

class VncFrameGrabber;
class VncFrame;
class RfbSocket;
class QImage;
class QPoint;

class RfbPixelStreamer
{
  public:
    RfbPixelStreamer();
    ~RfbPixelStreamer();

    void sendFrame( const VncFrameGrabber*, int quality, RfbSocket* );
    void sendCursor( const QPoint&, const QImage&, RfbSocket* );

    void sendServerFormat( RfbSocket* );
    void receiveClientFormat( RfbSocket* );

  private:
    void sendBytes( const VncFrame&, RfbSocket* );

  private:
    Q_DISABLE_COPY( RfbPixelStreamer )

    class PrivateData;
    PrivateData* m_data;
};
