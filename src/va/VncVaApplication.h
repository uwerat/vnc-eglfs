/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

class QSize;
class QRect;
class QByteArray;

class VncVaApplication
{
  public:
    static bool isValid();

    VncVaApplication( unsigned int texture );
    ~VncVaApplication();

    QByteArray encode( const QSize&, const QRect&, int quality );

  private:
    class Encoder;
    Encoder* m_encoder = nullptr;
};
