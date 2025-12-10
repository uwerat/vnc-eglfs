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

/*
    Downloading the texture does not happen in the scene graph thread
    and we can't use its context. So we need an extra thread holding
    a shared context and an offline surface.
 */

class VncTextureGrabber : public QThread
{
  public:
    VncTextureGrabber( QOpenGLContext*, QObject* parent = nullptr );
    ~VncTextureGrabber() override;

    VncFrame grabFrame( uint textureId, const QSize&,
        const QRect& subRect, int quality );

  protected:
    void run() override;

  private:
    VncFrame encodeFrame( const VncFrame&, int quality );
    VncFrame encodeFrame( uint textureId, const QSize&,
        const QRect& subRect, int quality );

    QOpenGLContext* m_context = nullptr;

    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    QAtomicInt m_abort {0};

    uint m_textureId = 0;

    QSize m_size;
    QRect m_subRect;
    int m_quality = 0;

    VncVaEncoder* m_encoder = nullptr;
    VncFrame m_frame;

    bool m_done = false;
};
