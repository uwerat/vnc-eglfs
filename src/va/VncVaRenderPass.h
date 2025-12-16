/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <va/va.h>
#include <qmap.h>

class QSize;

class VncVaRenderPass
{
  public:
    VncVaRenderPass( VADisplay );
    virtual ~VncVaRenderPass();

    void close();

    void createContext( const QSize&, VASurfaceID );
    void destroyContext();

    void run();

    VASurfaceID surface() const { return m_surface; }

  protected:
    template< typename T > void setVABuffer( VABufferType, const T& );
    void setBuffer( VABufferType, unsigned int size, const void* data );

    void destroyBuffer( VABufferType );

    const VADisplay m_display;
    VAConfigID m_config = VA_INVALID_ID;

    VASurfaceID m_surface = VA_INVALID_ID;
    VAContextID m_context = VA_INVALID_ID;

    VASurfaceID m_renderSurface = VA_INVALID_ID;

  private:
    void destroyBuffers();

    QMap< VABufferType, VABufferID > m_bufferMap;
};

template< typename T >
inline void VncVaRenderPass::setVABuffer( VABufferType bufferType, const T& param )
{
    setBuffer( bufferType, sizeof( T ), &param );
}
