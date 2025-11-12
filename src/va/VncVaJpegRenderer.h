/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <QSize>
#include <va/va.h>

class QSize;

class VncVaJpegRenderer
{
  public:
    VncVaJpegRenderer();
    ~VncVaJpegRenderer();

    void initialize( VADisplay, VAContextID );

    void encodeSurface( VASurfaceID, const QSize&, int quality, VABufferID );

  private:
    void updateBuffers( const QSize&, int quality, VABufferID );
    void destroyBuffers();
    
    VABufferID createBuffer( VABufferType, unsigned int size, const void* data );
    
    template< typename T >
    VABufferID addVABuffer( VABufferType, const T& );

    VADisplay m_display = 0;
    VAContextID m_contextId = VA_INVALID_ID;

    enum Buffer
    {
        MatrixBuffer,
        HuffmanBuffer,
        SliceBuffer,

        PictureParameterBuffer,

        HeaderParameter,
        HeaderData,

        NumBuffers
    };

    void destroyBuffer( Buffer );

    VABufferID m_buffers[ NumBuffers ] = { VA_INVALID_ID };

    QSize m_size;
    int m_quality = -1;
    VABufferID m_targetBuffer = VA_INVALID_ID;
};

template< typename T >
inline VABufferID VncVaJpegRenderer::addVABuffer(
    VABufferType bufferType, const T& param )
{   
    return createBuffer( bufferType, sizeof( T ), &param );
}
