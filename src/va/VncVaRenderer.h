#pragma once

#include <vector>
#include <va/va.h>

class VncVaRenderer
{
  public:
    VncVaRenderer();
    ~VncVaRenderer();

    void initialize( VADisplay, VAConfigID, VASurfaceID, VAContextID );

    void addBuffers( int width, int height, int quality );
    void removeBuffers();

    void render( const uint8_t* );

  private:
    template< typename T > VABufferID createBuffer( VABufferType, T& );
    VABufferID createBuffer( VABufferType, unsigned int size, void* data );

    template< typename T > void addBuffer( VABufferType, T& );
    void addBuffer( VABufferType, unsigned int size, void* data );

    void addPictureParameterBuffer( int width, int height, int quality );
    void addMatrixBuffer();
    void addHuffmanTableBuffer();
    void addSliceParameterBuffer();
    void addPackedHeaderBuffer( int width, int height, int quality );

    void uploadFrame( const unsigned char* );

    VADisplay m_display = 0;

    VAConfigID m_configId = 0;
    VASurfaceID m_surfaceId = 0;
    VAContextID m_contextId = 0;

    VABufferID m_codedBufferId;
    std::vector< VABufferID > m_buffers;
};

template< typename T >
inline void VncVaRenderer::addBuffer( VABufferType bufferType, T& param )
{
    addBuffer( bufferType, sizeof( param ), &param );
}

template< typename T >
inline VABufferID VncVaRenderer::createBuffer( VABufferType bufferType, T& param )
{
    return createBuffer( bufferType, sizeof( param ), &param );
}


