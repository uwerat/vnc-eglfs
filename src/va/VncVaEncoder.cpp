/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaEncoder.h"
#include "../VncFrame.h"
#include "../VncDmaBuffer.h"

#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>

#include <qdebug.h>
#include <qvarlengtharray.h>

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include <drm/drm_fourcc.h>

static inline int yuvSize( const QSize& size )
{
    const auto w = size.width();
    const auto h = size.height();

    return w * h + 2 * ceil( 0.5 * w ) * ceil( 0.5 * h );
}

static void uploadBGR( VADisplay vaDisplay,
    const uint8_t* bgr, VAImage& vaImage )
{
    const auto numLineBytes = vaImage.width * 4;
    const auto numRows = vaImage.height;

    uint8_t* buf = NULL;
    vaMapBuffer( vaDisplay, vaImage.buf, reinterpret_cast< void** >( &buf ) );

    auto scanLine = buf + vaImage.offsets[0];
    for ( int row = 0; row < numRows; row++ )
    {
        memcpy( scanLine, bgr, numLineBytes );

        scanLine += vaImage.pitches[0];
        bgr += numLineBytes;
    }

    vaUnmapBuffer( vaDisplay, vaImage.buf );
}

static bool hasConfig( VADisplay display, VAProfile profile, VAEntrypoint entry )
{
    QVarLengthArray< VAEntrypoint > entries( vaMaxNumEntrypoints( display ) );

    int numEntries = 0;

    const auto vaStatus = vaQueryConfigEntrypoints(
        display, profile, entries.data(), &numEntries );

    if ( vaStatus == VA_STATUS_SUCCESS )
    {
        for ( int i = 0; i < numEntries; i++ )
        {
            if ( entries[i] == entry )
                return true;
        }
    }

    return false;
}

VncVaEncoder::VncVaEncoder()
{
}

VncVaEncoder::~VncVaEncoder()
{
    close();
}

bool VncVaEncoder::isValid()
{
    VncVaEncoder encoder;
    encoder.openDisplay();

    return hasConfig( encoder.m_display, VAProfileNone, VAEntrypointVideoProc )
        && hasConfig( encoder.m_display, VAProfileJPEGBaseline, VAEntrypointEncPicture );
}

bool VncVaEncoder::open()
{
    if ( m_display )
        return true;

    openDisplay();

    {
        auto vaStatus = vaCreateConfig( m_display, VAProfileNone,
            VAEntrypointVideoProc, nullptr, 0, &m_pass[0].config );

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateConfig:" << vaErrorStr( vaStatus );
    }

    {
        VAConfigAttrib attrib[2];

        {
            attrib[0].type = VAConfigAttribRTFormat;
            attrib[1].type = VAConfigAttribEncJPEG;

            vaGetConfigAttributes( m_display, VAProfileJPEGBaseline,
                VAEntrypointEncPicture, &attrib[0], 2);

            // RT should be one of below.
            if ( !( attrib[0].value & VA_RT_FORMAT_YUV420 ) )
            {
                /* Did not find the supported RT format */
                assert(0);
            }

            VAConfigAttribValEncJPEG val;
            val.value = attrib[1].value;

            /* Set JPEG profile attribs */
            val.bits.arithmatic_coding_mode = 0;
            val.bits.progressive_dct_mode = 0;
            val.bits.non_interleaved_mode = 1;
            val.bits.differential_mode = 0;

            attrib[1].value = val.value;
        }


        auto vaStatus = vaCreateConfig( m_display, VAProfileJPEGBaseline,
            VAEntrypointEncPicture, attrib, 2, &m_pass[1].config );

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateConfig:" << vaErrorStr( vaStatus );
    }

    return true;
}

void VncVaEncoder::close()
{
    for ( auto& pass : m_pass )
    {
        if ( pass.config != VA_INVALID_ID )
        {
            vaDestroyConfig( m_display, pass.config  );
            pass.config = VA_INVALID_ID;
        }
    }

    closeDisplay();
}

void VncVaEncoder::setSize( const QSize& size )
{
    if ( size == m_size )
        return;

    m_size = size;

    for ( auto& pass : m_pass )
    {
        if ( pass.buffer != VA_INVALID_ID )
        {
            vaDestroyBuffer( m_display, pass.buffer );
            pass.buffer = VA_INVALID_ID;
        }

        if ( pass.context != VA_INVALID_ID )
        {
            vaDestroyContext( m_display, pass.context );
            pass.context = VA_INVALID_ID;
        }

        if ( pass.surface != VA_INVALID_ID )
        {
            vaDestroySurfaces( m_display, &pass.surface, 1 );
            pass.surface = VA_INVALID_ID;
        }
    }

    if ( m_size.isEmpty() )
        return;

    {
        VASurfaceAttrib attrib;
        attrib.type = VASurfaceAttribPixelFormat;
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        attrib.value.type = VAGenericValueTypeInteger;
        attrib.value.value.i = VA_FOURCC_NV12;

        auto vaStatus = vaCreateSurfaces(m_display, VA_RT_FORMAT_YUV420,
            size.width(), size.height(), &m_pass[1].surface, 1, &attrib, 1);

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaCreateContext( m_display, m_pass[1].config,
            size.width(), size.height(), VA_PROGRESSIVE,
            &m_pass[1].surface, 1, &m_pass[1].context);

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateContext:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaCreateBuffer( m_display, m_pass[1].context,
            VAEncCodedBufferType, yuvSize( m_size ), 1, nullptr, &m_pass[1].buffer);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateBuffer:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaCreateContext( m_display, m_pass[0].config,
            m_size.width(), m_size.height(),
            VA_PROGRESSIVE, &m_pass[1].surface, 1, &m_pass[0].context);

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateContext:" << vaErrorStr( vaStatus );
    }
}

bool VncVaEncoder::openDisplay()
{
    // using Qt - see gbm TODO ...
    const auto paths = { "renderD128", "card0", "renderD129", "card1" };

    for ( const auto name : paths )
    {
        const auto path = std::string( "/dev/dri/" ) + name;

        const auto fd = ::open( path.c_str(), O_RDWR);
        if ( fd >= 0 )
        {
            m_display = vaGetDisplayDRM( fd );
            if ( m_display )
            {
                qInfo() << "VA device:" << path;
                m_drmFd = fd;
                break;
            }

            ::close( fd );
        }
    }

    if ( m_display == nullptr )
        return false;

    {
        int major_ver, minor_ver;
        auto vaStatus = vaInitialize( m_display, &major_ver, &minor_ver );
        assert(vaStatus == VA_STATUS_SUCCESS);
    }

    return true;
}

void VncVaEncoder::closeDisplay()
{
    if ( m_display != 0 )
    {
        vaTerminate( m_display );
        m_display = 0;
    }

    if ( m_drmFd >= 0 )
    {
        ::close( m_drmFd );
        m_drmFd = -1;
    }
}

QByteArray VncVaEncoder::bufferData( VABufferID bufferId ) const
{
    VACodedBufferSegment* segment;

    auto vaStatus = vaMapBuffer( m_display, bufferId, (void**)( &segment ) );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaMapBuffer:" << vaErrorStr( vaStatus );

    QByteArray data;
    if ( !( segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK ) )
    {
        data.resize( segment->size );
        memcpy( data.data(), segment->buf, segment->size );
    }

    vaStatus = vaUnmapBuffer( m_display, bufferId );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaUnmapBuffer:" << vaErrorStr( vaStatus );

    return data;
}

void VncVaEncoder::setFrame( const VncDmaBuffer& dma )
{
    setSize( dma.size() );

    VASurfaceAttrib attribs[4] = {};

    auto attr = attribs;

    {
        attr->type = VASurfaceAttribUsageHint;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.type = VAGenericValueTypeInteger;
        //attr->value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
        attr->value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_VPP_READ;
        attr++;
    }

    {
        attr->type = VASurfaceAttribPixelFormat;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.type = VAGenericValueTypeInteger;
        attr->value.value.i = VA_FOURCC_BGRA;
        attr++;
    }

    {
        attr->type = VASurfaceAttribMemoryType;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.type = VAGenericValueTypeInteger;
        attr->value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2;
        attr++;
    }

    {

        VADRMPRIMESurfaceDescriptor desc = {};

        desc.fourcc = VA_FOURCC_RGBA;

#if 1
        // OUT ???
        desc.width = m_size.width();
        desc.height = m_size.height();
        desc.objects[0].size = dma.stride() * m_size.height();
#endif

        desc.num_objects = 1;

        desc.objects[0].fd = dma.fd();
        desc.objects[0].drm_format_modifier = dma.modifier();

        desc.num_layers = 1;

        auto& layer = desc.layers[0];
        layer.drm_format = DRM_FORMAT_RGBA8888;
        layer.num_planes = 1;
        layer.object_index[0] = 0;
        layer.pitch[0] = dma.stride();
        layer.offset[0] = dma.offset();

        attr->type = VASurfaceAttribExternalBufferDescriptor;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.value.p = &desc;
        attr++;
    }

    const auto count = attr - attribs;

    auto vaStatus = vaCreateSurfaces(
        m_display, VA_RT_FORMAT_RGB32, m_size.width(), m_size.height(),
        &m_pass[0].surface, 1, attribs, count  );

    if( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );

#if 0
    {
        VADRMPRIMESurfaceDescriptor desc;
        vaStatus = vaExportSurfaceHandle( m_display, m_pass[0].surface,
            VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2, VA_EXPORT_SURFACE_READ_ONLY, &desc );

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaExportSurfaceHandle:" << vaErrorStr( vaStatus );

        qDebug() << "DMA" << desc.height
                 << desc.layers[0].pitch[0] << desc.layers[1].offset[0];
    }
#endif

}

void VncVaEncoder::setFrame( const VncFrame& frame )
{
    VAStatus vaStatus;

    setSize( frame.size() );

    {
        VASurfaceAttrib attrib;
        attrib.type = VASurfaceAttribPixelFormat;
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        attrib.value.type = VAGenericValueTypeInteger;
        attrib.value.value.i = VA_FOURCC_BGRA; // why not VA_FOURCC_RGBA

        vaStatus = vaCreateSurfaces( m_display, VA_RT_FORMAT_RGB32,
            m_size.width(), m_size.height(), &m_pass[0].surface, 1, &attrib, 1 );

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );

    }

    {
        VAImage vaImage;

        vaStatus = vaDeriveImage( m_display, m_pass[0].surface, &vaImage );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaDeriveImage:" << vaErrorStr( vaStatus );

        const auto bytes = frame.bytes();
        uploadBGR( m_display, bytes, vaImage );

        vaStatus = vaDestroyImage( m_display, vaImage.image_id );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaDestroyImage:" << vaErrorStr( vaStatus );
    }
}

VncFrame VncVaEncoder::encode( const QRect& subRect, int quality )
{
    Q_UNUSED( subRect );

    {
        VAProcPipelineParameterBuffer params = {};
        params.surface = m_pass[0].surface;

        auto vaStatus = vaCreateBuffer( m_display, m_pass[0].context,
            VAProcPipelineParameterBufferType, sizeof( params ), 1,
            &params, &m_pass[0].buffer );

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateBuffer:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaBeginPicture( m_display, m_pass[0].context, m_pass[1].surface );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

        vaStatus = vaRenderPicture( m_display, m_pass[0].context, &m_pass[0].buffer, 1);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

        vaStatus = vaEndPicture( m_display, m_pass[0].context );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaEndPicture:" << vaErrorStr( vaStatus );

        vaStatus = vaSyncSurface( m_display, m_pass[1].surface);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );
    }

    vaDestroySurfaces( m_display, &m_pass[0].surface, 1);

    m_encoder.initialize( m_display, m_pass[1].context );
    m_encoder.encodeSurface( m_pass[1].surface, m_size, quality, m_pass[1].buffer );

    const auto encodedBytes = bufferData( m_pass[1].buffer );

    return VncFrame::fromByteArray( VncFrame::Jpeg,
        m_size.width(), m_size.height(), encodedBytes );
}
