/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <va/va.h>

class QRect;
class QSize;

namespace VncEgl { class DmaBuffer; }

class VncVaConverter
{
  public:
    VncVaConverter();
    ~VncVaConverter();

    void render( VASurfaceID );
    void updateContext( const QSize& );

    void setSource( VncEgl::DmaBuffer&, const QSize&, const QRect& );

  private:
    void updatePipeline( VASurfaceID );

    VAConfigID m_config = VA_INVALID_ID;
    VAContextID m_context = VA_INVALID_ID;

    VASurfaceID m_textureSurface = VA_INVALID_ID;
    VABufferID m_pipelineBuffer = VA_INVALID_ID;
};
