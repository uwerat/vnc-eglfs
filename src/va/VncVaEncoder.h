/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qrect.h>
#include <va/va.h>

class VncDmaBuffer;
class VncVaConverterPass;
class VncVaEncoderPass;
class QByteArray;

class VncVaEncoder
{
  public:
    VncVaEncoder();
    ~VncVaEncoder();

    static bool isValid();

    bool open();
    void close();

    QByteArray encode( unsigned int texture, const QSize&, const QRect&, int quality );

  private:
    bool openDisplay();
    void closeDisplay();

    VADisplay m_display = 0;
    int m_drmFd = -1;

    VncVaConverterPass* m_converter = nullptr;
    VncVaEncoderPass* m_encoder = nullptr;

    QSize m_size;
};
