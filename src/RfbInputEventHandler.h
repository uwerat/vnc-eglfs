/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qglobal.h>

class QPointF;
class QWindow;

namespace Rfb
{
    void handlePointerEvent( const QPointF&, quint8 buttonMask, QWindow* );
    void handleKeyEvent( quint32 key, bool down, QWindow* );
}
