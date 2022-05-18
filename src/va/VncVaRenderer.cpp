#include "VncVaRenderer.h"
#include "VncJpeg.h"

#include <va/va_enc_jpeg.h>

#include <cstring>
#include <cstdio>

VncVaRenderer::VncVaRenderer()
{
}

VncVaRenderer::~VncVaRenderer()
{
}

void VncVaRenderer::initialize( VADisplay display, VAConfigID configId,
    VASurfaceID surfaceId, VAContextID contextId )
{
    m_display = display;
    m_configId = configId;
    m_surfaceId = surfaceId;
    m_contextId = contextId;
}

void VncVaRenderer::addBuffers( int width, int height, int quality )
{
    m_codedBufferId = createBuffer( VAEncCodedBufferType, 4 * width * height, nullptr );

    addPictureParameterBuffer( width, height, quality );
    addMatrixBuffer();
    addHuffmanTableBuffer();
    addSliceParameterBuffer();
    addPackedHeaderBuffer( width, height, quality );
}

void VncVaRenderer::removeBuffers()
{
    for ( const auto id : m_buffers )
        vaDestroyBuffer( m_display, id );

    m_buffers.clear();

    // what about m_codedBufferId ???
}

void VncVaRenderer::render( const uint8_t* frame )
{
    uploadFrame( frame );

    vaBeginPicture( m_display, m_contextId, m_surfaceId );

    for ( auto& id : m_buffers )
        vaRenderPicture( m_display, m_contextId, &id, 1 );

    vaEndPicture( m_display, m_contextId );

    vaSyncSurface( m_display, m_surfaceId );

    auto surface_status = static_cast< VASurfaceStatus >( 0 );
    vaQuerySurfaceStatus( m_display, m_surfaceId, &surface_status );
    // check surface_status ???
}

void VncVaRenderer::mapEncoded( uint8_t*& data, size_t& size )
{
    // segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK ???

    VACodedBufferSegment* segment = nullptr;
    vaMapBuffer( m_display, m_codedBufferId, (void**)( &segment ) );

    data = reinterpret_cast< uint8_t* >( segment->buf );
    size = segment->size;
}

void VncVaRenderer::unmapEncoded()
{
    vaUnmapBuffer( m_display, m_codedBufferId );
}

void VncVaRenderer::uploadFrame( const unsigned char* frame )
{
    VAImage image;

    vaDeriveImage( m_display, m_surfaceId, &image);

    void* surface = nullptr;
    vaMapBuffer( m_display, image.buf, &surface );

    auto src = frame;
    auto dst = reinterpret_cast< unsigned char* >( surface ) + image.offsets[0];

    for ( int row = 0; row < image.height; row++ )
    {
        memcpy( dst, src, image.width * 4 );

        dst += image.pitches[0];
        src += image.width * 4;
    }

    vaUnmapBuffer( m_display, image.buf );
    vaDestroyImage( m_display, image.image_id);
}

VABufferID VncVaRenderer::createBuffer(
    VABufferType bufferType, unsigned int size, void* data )
{
    VABufferID bufferId = 0;

    const auto status = vaCreateBuffer( m_display, m_contextId,
        bufferType, size, 1, data, &bufferId );

    if ( status != VA_STATUS_SUCCESS )
    {
        fprintf( stderr, "vaCreateBuffer %d:  %s\n",
            bufferType, vaErrorStr( status ) );
    }

    return bufferId;
}

void VncVaRenderer::addBuffer(
    VABufferType bufferType, unsigned int size, void* data )
{
    const auto bufferId = createBuffer( bufferType, size, data );
    m_buffers.push_back( bufferId );
}

void VncVaRenderer::addPictureParameterBuffer( int width, int height, int quality )
{
    VAEncPictureParameterBufferJPEG param = {};

    param.reconstructed_picture = 0;
    param.coded_buf = m_codedBufferId;

    param.picture_width = width;
    param.picture_height = height;
    param.quality = quality;

    // 0 - Baseline, 1 - Extended, 2 - Lossless, 3 - Hierarchical
    param.pic_flags.bits.profile = 0;

    // 0 - sequential, 1 - extended, 2 - progressive
    param.pic_flags.bits.progressive = 0;

    // 0 - arithmetic, 1 - huffman
    param.pic_flags.bits.huffman = 1;

    param.pic_flags.bits.interleaved = 0;
    param.pic_flags.bits.differential = 0;

    param.sample_bit_depth = 8;
    param.num_scan = 1;

    param.num_components = 3;
    param.component_id[0] = 0;
    param.component_id[1] = 1;
    param.component_id[2] = 2;

    param.quantiser_table_selector[0] = 0;
    param.quantiser_table_selector[1] = 1;
    param.quantiser_table_selector[2] = 1;

    param.quality = quality;

    addBuffer( VAEncPictureParameterBufferType, param );
}

void VncVaRenderer::addMatrixBuffer()
{
    VAQMatrixBufferJPEG param = {};

    param.load_lum_quantiser_matrix = 1;
    VncJpeg::lumaQuantization().copyTo( param.lum_quantiser_matrix );

    param.load_chroma_quantiser_matrix = 1;
    VncJpeg::chromaQuantization().copyTo( param.chroma_quantiser_matrix );

    addBuffer( VAQMatrixBufferType, param );
}

void VncVaRenderer::addHuffmanTableBuffer()
{
    VAHuffmanTableBufferJPEGBaseline param = {};

    param.load_huffman_table[0] = 1;
    param.load_huffman_table[1] = 1;

    {
        auto& table = param.huffman_table[0];

        VncJpeg::dcCoefficientsLuminance().copyTo( table.num_dc_codes );
        VncJpeg::dcValues().copyTo( table.dc_values );

        VncJpeg::acCoefficientsLuminance().copyTo( table.num_ac_codes );
        VncJpeg::acValuesLuminance().copyTo( table.ac_values );

        //memset( table.pad, 0, 2);
    }

    {
        auto& table = param.huffman_table[1];

        VncJpeg::dcCoefficientsChroma().copyTo( table.num_dc_codes );
        VncJpeg::dcValues().copyTo( table.dc_values );

        VncJpeg::acCoefficientsChroma().copyTo( table.num_ac_codes );
        VncJpeg::acValuesChroma().copyTo( table.ac_values );

        //memset( table.pad, 0, 2 );
    }

    addBuffer( VAHuffmanTableBufferType, param );
}

void VncVaRenderer::addSliceParameterBuffer()
{
    VAEncSliceParameterBufferJPEG param;

    param.restart_interval = 0;
    param.num_components = 3;

    param.components[0].component_selector = 1;
    param.components[0].dc_table_selector = 0;
    param.components[0].ac_table_selector = 0;

    param.components[1].component_selector = 2;
    param.components[1].dc_table_selector = 1;
    param.components[1].ac_table_selector = 1;

    param.components[2].component_selector = 3;
    param.components[2].dc_table_selector = 1;
    param.components[2].ac_table_selector = 1;

    addBuffer( VAEncSliceParameterBufferType, param );
}

void VncVaRenderer::addPackedHeaderBuffer( int width, int height, int quality )
{
    const VncJpeg::Header header( width, height, quality );

    VAEncPackedHeaderParameterBuffer param;
    param.type = VAEncPackedHeaderRawData;
    param.bit_length = header.count();
    param.has_emulation_bytes = 0;

    addBuffer( VAEncPackedHeaderParameterBufferType, param );

    const auto count = ( header.count() + 7 ) / 8;
    const auto data = ( void* )header.buffer();

    addBuffer( VAEncPackedHeaderDataBufferType, count, data );
}
