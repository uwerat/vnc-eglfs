/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncVaJpegRenderer.h"

#include <qrect.h>
#include <va/va.h>

class VncFrame;
class QByteArray;

class VncVaEncoder
{
  public:
    VncVaEncoder();
    ~VncVaEncoder();

    bool open();
    void close();

    void setSize( const QSize& );

    VncFrame encode( const VncFrame&, int quality );
    VncFrame encode( uint textureId, const QSize&, const QRect&, int quality );

  private:
    VncFrame encodeBytes( const uint8_t* bytes, const QSize&, int quality );

    bool openDisplay();
    void closeDisplay();

    QByteArray bufferData( VABufferID ) const;

    VADisplay m_display = 0;
    int m_drmFd = -1;

    VAConfigID m_configId = VA_INVALID_ID;
    VASurfaceID m_yuvSurfaceId = VA_INVALID_ID;

    VAContextID m_contextId = VA_INVALID_ID;
    VABufferID m_jpegBufferId = VA_INVALID_ID;

    QSize m_size;
    VncVaJpegRenderer m_encoder;
};
