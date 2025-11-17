/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncFrame.h"
#include <qobject.h>

#include <memory>

class QRect;
class QSize;

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
    class PrivateData;
    std::unique_ptr< PrivateData > m_data;
};
