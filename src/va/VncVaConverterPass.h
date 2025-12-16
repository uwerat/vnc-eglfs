/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncVaRenderPass.h"

class QRect;

class VncVaConverterPass final : public VncVaRenderPass
{
  public:
    VncVaConverterPass( VADisplay );
    ~VncVaConverterPass() override;

    void createSurface( unsigned int, const QSize&, const QRect& );
    void updateBuffers();

  private:
    void createConfig();
};
