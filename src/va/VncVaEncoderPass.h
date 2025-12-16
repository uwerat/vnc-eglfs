/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncVaRenderPass.h"

class VncVaEncoderPass final : public VncVaRenderPass
{
  public:
    VncVaEncoderPass( VADisplay );
    ~VncVaEncoderPass() override;

    QByteArray encodedData() const;

    void createSurface( const QSize& );

    void createTargetBuffer( const QSize& );
    void destroyTargetBuffer();

    void updateBuffers( const QSize&, int quality );

  private:
    void createConfig();

    VABufferID m_targetBuffer = VA_INVALID_ID;
};
