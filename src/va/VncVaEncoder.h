/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include "VncVaRenderer.h"
#include <qsize.h>

class VncVaEncoder final : public VncVaRenderer
{
    using Inherited = VncVaRenderer;

  public:
    VncVaEncoder( VADisplay );
    ~VncVaEncoder() override;

    void run();
    QByteArray encodedData() const;

    void updateContext( const QSize&, VASurfaceID );

    void resizeSurface( const QSize& );
    VASurfaceID surface() const { return m_surface; }
    QSize surfaceSize() const { return m_size; }

    void updateParameters( int quality );

  private:
    enum ParameterBuffer
    {
        Matrix,
        Huffman,
        Slice,

        Header,
        HeaderData,

        Picture,

        NumBuffers
    };

    QSize m_size;
    VASurfaceID m_surface = VA_INVALID_ID;
    VABufferID m_renderBuffer = VA_INVALID_ID;
};
