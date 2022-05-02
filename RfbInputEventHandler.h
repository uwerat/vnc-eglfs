/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
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
