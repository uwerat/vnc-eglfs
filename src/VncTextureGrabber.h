/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncFrame.h"

#include <qrect.h>
#include <qmutex.h>
#include <qthread.h>

#include <qwaitcondition.h>
#include <QAtomicInt>

class QOpenGLContext;

class VncTextureGrabber : public QThread
{
  public:
    VncTextureGrabber( QObject* parent = nullptr );
    ~VncTextureGrabber() override;

    bool supportsVideoAcceleration() const;
    VncFrame grabFrame( const QRect& subRect, int quality );

    void importBackBuffer( const QSize& );

    static bool isSupported( const QOpenGLContext* );

  protected:
    void run() override;

  private:
    QOpenGLContext* m_context = nullptr;

    class FrameBufferObject;
    FrameBufferObject* m_fbo = nullptr;

    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    QAtomicInt m_abort {0};

    QRect m_subRect;
    int m_quality = 0;

    VncFrame m_frame;

    bool m_done = false;
    bool m_requested = false;

    bool m_videoAcceleration = false;
};

inline bool VncTextureGrabber::supportsVideoAcceleration() const
{
    return m_videoAcceleration;
}
