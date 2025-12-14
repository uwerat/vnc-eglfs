/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncFrame.h"
#include <qobject.h>

class QRect;
class QSize;
class VncTextureGrabber;

class VncFrameGrabber : public QObject
{
  public:
    VncFrameGrabber( QObject* parent = nullptr );
    ~VncFrameGrabber();

    bool isValid() const;

    void update( const QSize& );
    void invalidate();

    QSize frameSize() const;

    VncFrame frame( VncFrame::Encoding, int qualityLevel = 5 ) const;
    VncFrame subFrame( const QRect&, VncFrame::Encoding, int qualityLevel = 5 ) const;

  private:
    VncTextureGrabber* m_textureGrabber = nullptr;
    QSize m_size;

    class Cache;
    Cache* m_cache = nullptr;
};
