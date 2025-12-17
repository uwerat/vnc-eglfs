/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <va/va.h>
#include <qvector.h>

class QSize;
class QByteArray;

class VncVaRenderer
{
  public:
    VncVaRenderer( VADisplay );
    virtual ~VncVaRenderer();

    VADisplay display() const { return m_display; }

  protected:

    void createConfig( VAProfile, VAEntrypoint,
        const VAConfigAttrib* = nullptr, int count = 0 );

    void createContext( const QSize&, VASurfaceID );
    void destroyContext();

    VASurfaceID createSurface( unsigned int format,
        const QSize&, const VASurfaceAttrib*, int count );
    void destroySurface( VASurfaceID );

    VABufferID createBuffer( VABufferType, unsigned int size, void* data = nullptr );
    void destroyBuffer( VABufferID );

    QByteArray bufferData( VABufferID ) const;

#if 1
    void setParameterCount( int );

    template< typename T > void setParameterBuffer( int index, VABufferType, const T& );
    void setParameterBuffer( int index, VABufferType, unsigned int size, const void* data );

    void render( VASurfaceID );
#endif

  private:
    const VADisplay m_display;
    VAConfigID m_config = VA_INVALID_ID;
    VAContextID m_context = VA_INVALID_ID;

    QVector< VABufferID > m_parameterBuffers;
};

template< typename T >
inline void VncVaRenderer::setParameterBuffer(
    int index, VABufferType bufferType, const T& param )
{
    setParameterBuffer( index, bufferType, sizeof( T ), &param );
}
