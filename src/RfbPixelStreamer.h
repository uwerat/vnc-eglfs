/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <memory>
#include <qglobal.h>

class VncFrame;
class RfbSocket;
class QImage;
class QPoint;
class QSize;

class RfbPixelStreamer
{
  public:
    RfbPixelStreamer();
    ~RfbPixelStreamer();

    void sendFrames( const VncFrame*, int count, RfbSocket* );
    void sendCursor( const QPoint&, const QImage&, RfbSocket* );

    void sendServerFormat( RfbSocket* );
    void receiveClientFormat( RfbSocket* );

  private:
    void sendBytesRgb( const QSize&, const uint8_t*, RfbSocket* );
    void sendBytesTight( const VncFrame&, RfbSocket* );

  private:
    Q_DISABLE_COPY( RfbPixelStreamer )

    class PrivateData;
    PrivateData* m_data;
};
