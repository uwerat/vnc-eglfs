/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qthread.h>
#include <memory>

class VncFrame;
class QRect;

class VncFrameGrabber : public QThread
{
  public:
    VncFrameGrabber( QObject* parent = nullptr );
    ~VncFrameGrabber() override;

    bool supportsVideoAcceleration() const;
    VncFrame grabFrame( const QRect& region, int quality );

    void importBackBuffer( const QSize& );

  protected:
    void run() override;

  private:
    class PrivateData;
    std::unique_ptr< PrivateData > m_data;
};
