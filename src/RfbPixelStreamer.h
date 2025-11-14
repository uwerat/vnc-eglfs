/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qvector.h>
#include <memory>

class RfbSocket;
class QImage;
class QRect;
class QPoint;

class RfbPixelStreamer
{
  public:
    RfbPixelStreamer();
    ~RfbPixelStreamer();

    void sendImageRaw( const QImage&,
        const QVector< QRect >&, RfbSocket* );

    void sendImageJPEG( const QImage&,
        const QVector< QRect >&, int qualityLevel, RfbSocket* );

    void sendCursor( const QPoint&, const QImage&, RfbSocket* );

    void sendServerFormat( RfbSocket* );
    void receiveClientFormat( RfbSocket* );

  private:
    void sendImageData( const QImage&, const QRect&, RfbSocket* );

  private:
    Q_DISABLE_COPY( RfbPixelStreamer )

    class PrivateData;
    PrivateData* m_data;
};
