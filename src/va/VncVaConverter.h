/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncVaRenderer.h"

class QRect;

class VncVaConverter final : public VncVaRenderer
{
    using Inherited = VncVaRenderer;

  public:
    VncVaConverter( VADisplay );
    ~VncVaConverter() override;

    void run();
    void updateContext( const QSize&, VASurfaceID );

    void setTexture( unsigned int, const QSize&, const QRect& );
    void updateParameters();

  private:
    VASurfaceID m_surfaces[2] = { VA_INVALID_ID };
};
