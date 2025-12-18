/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qrect.h>
#include <va/va.h>

class VncVaConverter;
class VncVaEncoder;

class QByteArray;

class VncVaApplication
{
  public:
    VncVaApplication();
    ~VncVaApplication();

    static bool isValid();

    void open();
    void close();

    QByteArray encode( unsigned int texture, const QSize&, const QRect&, int quality );

  private:
    VADisplay m_display = 0;
    int m_drmFd = -1;

    VncVaConverter* m_converter = nullptr;
    VncVaEncoder* m_encoder = nullptr;
};
