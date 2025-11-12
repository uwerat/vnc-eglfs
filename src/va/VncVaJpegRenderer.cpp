/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaJpegRenderer.h"
#include "VncJpeg.h"

#include <QSize>
#include <QVector>
#include <QDebug>

#include <va/va_enc_jpeg.h>

VABufferID VncVaJpegRenderer::createBuffer(
    VABufferType bufferType, unsigned int size, const void* data )
{
    VABufferID bufferId = 0;

    const auto status = vaCreateBuffer( m_display, m_contextId,
        bufferType, size, 1, const_cast< void* >( data ), &bufferId );

    if ( status != VA_STATUS_SUCCESS )
    {
        fprintf( stderr, "vaCreateBuffer %d:  %s\n",
            bufferType, vaErrorStr( status ) );
    }

    return bufferId;
}

VncVaJpegRenderer::VncVaJpegRenderer( )
{
}

VncVaJpegRenderer::~VncVaJpegRenderer()
{
    destroyBuffers();
}

void VncVaJpegRenderer::initialize( VADisplay display, VAContextID contextId )
{
    if ( m_display == display && m_contextId == contextId )
        return;

    destroyBuffers();

    m_display = display;
    m_contextId = contextId;
}

void VncVaJpegRenderer::destroyBuffers()
{
    for ( auto& bufferId : m_buffers )
    {
        if ( bufferId != VA_INVALID_ID )
        {
            vaDestroyBuffer( m_display, bufferId );
            bufferId = VA_INVALID_ID;
        }
    }
}

void VncVaJpegRenderer::destroyBuffer( Buffer buffer )
{
    auto& bufferId = m_buffers[buffer];
    if ( bufferId != VA_INVALID_ID )
    {
        vaDestroyBuffer( m_display, bufferId );
        bufferId = VA_INVALID_ID;
    }
}

static inline void copyTo( const std::vector< uint8_t >& from, uint8_t* to )
{   
    memcpy( to, from.data(), from.size() );
}

void VncVaJpegRenderer::updateBuffers(
    const QSize& size, int quality, VABufferID targetId )
{
    /*
        VAQMatrixBufferType and VAHuffmanTableBufferType do not depend on
        the size/quality need to be defined only once. Actually many drivers
        have default settings for these tables and it is not necessary to
        upload them at all.

        In my test environment the driver has only default settings for
        VAQMatrixBufferType. Not very reliable - so we better upload
        both buffers.

        Even worse: in my environment I have the options to never upload the
        matrix buffer or to do it for each run. No idea why.
     */

#if 1
    destroyBuffer( MatrixBuffer ); // see above
#endif
    if ( m_buffers[ MatrixBuffer ] == VA_INVALID_ID )
    {
        VAQMatrixBufferJPEG p;

        p.load_lum_quantiser_matrix = 1;
        copyTo( VncJpeg::lumaQuantization, p.lum_quantiser_matrix );

        p.load_chroma_quantiser_matrix = 1;
        copyTo( VncJpeg::chromaQuantization, p.chroma_quantiser_matrix );

        m_buffers[MatrixBuffer] = addVABuffer( VAQMatrixBufferType, p );
    }


    if ( m_buffers[ HuffmanBuffer ] == VA_INVALID_ID )
    {
        VAHuffmanTableBufferJPEGBaseline p = {};

        p.load_huffman_table[0] = 1; //Load Luma Hufftable
        p.load_huffman_table[1] = 1; //Load Chroma Hufftable for other formats

        p.load_huffman_table[0] = 1; 
        p.load_huffman_table[1] = 1;
        
        for ( int i = 0; i < 2; i++ )
        {
            auto& table = p.huffman_table[i];
        
            using namespace VncJpeg;

            copyTo( dcValues, table.dc_values );
        
            if ( i == 0 )
            {
                copyTo( acValuesLuminance, table.ac_values );
                copyTo( dcCoefficientsLuminance, table.num_dc_codes );
                copyTo( acCoefficientsLuminance, table.num_ac_codes );
            }
            else
            {
                copyTo( acValuesChroma, table.ac_values );
                copyTo( dcCoefficientsChroma, table.num_dc_codes );
                copyTo( acCoefficientsChroma, table.num_ac_codes );
            }
        }

        m_buffers[HuffmanBuffer] = addVABuffer( VAHuffmanTableBufferType, p );
    }

    if ( m_buffers[ SliceBuffer ] == VA_INVALID_ID )
    {
        constexpr VAEncSliceParameterBufferJPEG p =
            { 0, 3, { { 1, 0, 0 }, { 2, 1, 1 }, { 3, 1, 1 } }, {} };

        m_buffers[SliceBuffer] = addVABuffer( VAEncSliceParameterBufferType, p );
    }

    if ( ( size != m_size ) || ( quality != m_quality )
        || ( targetId != m_targetBuffer ) )
    {
        m_size = size;
        m_quality = quality;
        m_targetBuffer = targetId;

        {
            destroyBuffer( PictureParameterBuffer );

            VAEncPictureParameterBufferJPEG param =
            {
                0, uint16_t( size.width() ), uint16_t( size.height() ), targetId,
                { 0, 0, 1, 0, 0 }, 8, 1, 3, { 0, 1, 2 }, { 0, 1, 1 }, uint8_t( quality ), {}
            };

            m_buffers[PictureParameterBuffer] =
                 addVABuffer( VAEncPictureParameterBufferType, param );
        } 

        {
            destroyBuffer( HeaderParameter );
            destroyBuffer( HeaderData );

            const VncJpeg::Header header( size.width(), size.height(), quality );

            VAEncPackedHeaderParameterBuffer param =
                { VAEncPackedHeaderRawData, uint32_t(header.count()) * 8, 0, {} };

            m_buffers[HeaderParameter] =
                addVABuffer( VAEncPackedHeaderParameterBufferType, param );

            m_buffers[HeaderData] = createBuffer( VAEncPackedHeaderDataBufferType,
                header.count(), const_cast< uint8_t* >( header.buffer() ) );
        }
    }
}

void VncVaJpegRenderer::encodeSurface( VASurfaceID surfaceId,
    const QSize& size, int quality, VABufferID targetId )
{
    updateBuffers( size, quality, targetId );

    auto vaStatus = vaBeginPicture( m_display, m_contextId, surfaceId );

    if ( vaStatus != VA_STATUS_SUCCESS )
        fprintf( stderr, "vaBeginPicture:  %s\n", vaErrorStr( vaStatus ) );

    vaRenderPicture( m_display, m_contextId,
        m_buffers, sizeof( m_buffers ) / sizeof( m_buffers[0] ) );

    if ( vaStatus != VA_STATUS_SUCCESS )
        fprintf( stderr, "vaRenderPicture:  %s\n", vaErrorStr( vaStatus ) );

    vaStatus = vaEndPicture( m_display, m_contextId );

    if ( vaStatus != VA_STATUS_SUCCESS )
        fprintf( stderr, "vaEndPicture:  %s\n", vaErrorStr( vaStatus ) );

    vaStatus = vaSyncSurface( m_display, surfaceId );

    if ( vaStatus != VA_STATUS_SUCCESS )
        fprintf( stderr, "vaSyncSurface:  %s\n", vaErrorStr( vaStatus ) );
}
