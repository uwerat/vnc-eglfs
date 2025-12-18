/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <va/va.h>

class QRect;
class QSize;

class VncVaConverter
{
  public:
    VncVaConverter();
    ~VncVaConverter();

    void run();
    void updateContext( const QSize&, VASurfaceID );

    void setTexture( unsigned int, const QSize&, const QRect& );
    void updateParameters();

  private:
    VAConfigID m_config = VA_INVALID_ID;
    VAContextID m_context = VA_INVALID_ID;

    VASurfaceID m_surfaces[2] = { VA_INVALID_ID, VA_INVALID_ID };
    VABufferID m_pipelineBuffer = VA_INVALID_ID;
};
