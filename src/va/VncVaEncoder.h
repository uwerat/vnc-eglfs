/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncVaJpegRenderer.h"

#include <qrect.h>
#include <va/va.h>

class VncFrame;
class VncDmaBuffer;
class QByteArray;

class VncVaEncoder
{
  public:
    VncVaEncoder();
    ~VncVaEncoder();

    bool open();
    void close();

    void setSize( const QSize& );

    void setFrame( const VncFrame& );
    void setFrame( const VncDmaBuffer& );

    VncFrame encode( const QRect&, int quality );

  private:
    VncFrame encodeBytes( const uint8_t* bytes, const QSize&, int quality );

    bool openDisplay();
    void closeDisplay();

    QByteArray bufferData( VABufferID ) const;

    VADisplay m_display = 0;
    int m_drmFd = -1;

    struct
    {
        VAConfigID config = VA_INVALID_ID;
        VASurfaceID surface = VA_INVALID_ID;
        VAContextID context = VA_INVALID_ID;
        VABufferID buffer = VA_INVALID_ID;

    } m_pass[2];

    QSize m_size;
    VncVaJpegRenderer m_encoder;
};
