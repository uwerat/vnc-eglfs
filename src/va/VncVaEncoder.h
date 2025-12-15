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

    static bool isValid();

    bool open();
    void close();

    void setFrame( const VncDmaBuffer&, const QRect& );
    VncFrame encode( int quality );

  private:
    void setSize( const QSize& );

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
