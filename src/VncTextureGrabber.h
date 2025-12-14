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
class VncVaEncoder;

class VncTextureGrabber : public QThread
{
  public:
    VncTextureGrabber( QObject* parent = nullptr );
    ~VncTextureGrabber() override;

    VncFrame grabFrame( const QRect& subRect, int quality );

    void importBackBuffer( const QSize& );

    static bool isSupported( const QOpenGLContext* );

  protected:
    void run() override;

  private:
    VncFrame encodeFrame( const QRect& subRect, int quality );
    VncFrame readFrame();

    QOpenGLContext* m_context = nullptr;

    class FrameBufferObject;
    FrameBufferObject* m_fbo = nullptr;

    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    QAtomicInt m_abort {0};

    QRect m_subRect;
    int m_quality = 0;

    VncVaEncoder* m_encoder = nullptr;
    VncFrame m_frame;

    bool m_done = false;
    bool m_requested = false;
};
