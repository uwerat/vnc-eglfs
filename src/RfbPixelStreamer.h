/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <qregion.h>
#include <memory>

class RfbSocket;
class QImage;

class RfbPixelStreamer
{
  public:
    RfbPixelStreamer();
    ~RfbPixelStreamer();

    void sendImageRaw( const QImage&, const QRegion&, RfbSocket* );
    void sendImageJPEG( const QImage&, const QRegion&, int qualityLevel, RfbSocket* );

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
