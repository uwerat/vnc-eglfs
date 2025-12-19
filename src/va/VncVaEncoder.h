/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qsize.h>
#include <va/va.h>

class VncVaEncoder
{
  public:
    VncVaEncoder();
    ~VncVaEncoder();

    void render( VASurfaceID );
    QByteArray encodedData() const;

    void updateContext( const QSize& );
    void updateParameters( const QSize&, int quality );

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

    template< typename T > void setParameterBuffer( int index, VABufferType, const T& );
    void setParameterData( int index, VABufferType, unsigned int size, const void* );

    VAConfigID m_config = VA_INVALID_ID;
    VAContextID m_context = VA_INVALID_ID;

    VABufferID m_renderBuffer = VA_INVALID_ID;
    VABufferID m_buffers[ NumBuffers ];
};

template< typename T >
inline void VncVaEncoder::setParameterBuffer(
    int index, VABufferType bufferType, const T& param )
{
    setParameterData( index, bufferType, sizeof( T ), &param );
}
