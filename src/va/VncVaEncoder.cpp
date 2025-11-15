/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaEncoder.h"

#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>

#include <QImage>
#include <QDebug>

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drm.h>

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

static void convertToNV12( VADisplay display, const QSize& size,
    VASurfaceID pixelSurface, VASurfaceID yuvSurface )
{
    VAConfigID config;
    auto vaStatus = vaCreateConfig( display, VAProfileNone,
        VAEntrypointVideoProc, nullptr, 0, &config);
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateConfig:" << vaErrorStr( vaStatus );

    VAContextID context;
    vaStatus = vaCreateContext( display, config, size.width(), size.height(),
        VA_PROGRESSIVE, &yuvSurface, 1, &context);
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateContext:" << vaErrorStr( vaStatus );

    VABufferID yuvBufferID;
    {
        VAProcPipelineParameterBuffer params = {};
        params.surface = pixelSurface;

        vaStatus = vaCreateBuffer( display, context,
            VAProcPipelineParameterBufferType, sizeof( params ), 1, &params, &yuvBufferID );

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateBuffer:" << vaErrorStr( vaStatus );
    }

    {
        vaStatus = vaBeginPicture( display, context, yuvSurface);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

        vaStatus = vaRenderPicture( display, context, &yuvBufferID, 1);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

        vaStatus = vaEndPicture( display, context);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaEndPicture:" << vaErrorStr( vaStatus );
    }

    vaStatus = vaSyncSurface( display, yuvSurface);
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

    vaDestroyContext( display, context);
    vaDestroyConfig( display, config);
}

VncVaEncoder::VncVaEncoder()
{
}

VncVaEncoder::~VncVaEncoder()
{
    close();
}

bool VncVaEncoder::open()
{
    openDisplay();

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


    auto vaStatus = vaCreateConfig(m_display, VAProfileJPEGBaseline,
        VAEntrypointEncPicture, attrib, 2, &m_configId);
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateConfig:" << vaErrorStr( vaStatus );

    return true;
}

void VncVaEncoder::close()
{
    if ( m_configId != VA_INVALID_ID )
    {
        vaDestroyConfig( m_display, m_configId );
        m_configId = VA_INVALID_ID;
    }

    closeDisplay();
}

void VncVaEncoder::setSize( const QSize& size )
{
    if ( size == m_size )
        return;

    m_size = size;

    if ( m_jpegBufferId != VA_INVALID_ID )
    {
        vaDestroyBuffer( m_display, m_jpegBufferId );
        m_jpegBufferId = VA_INVALID_ID;
    }

    if ( m_contextId != VA_INVALID_ID )
    {
        vaDestroyContext( m_display, m_contextId );
        m_contextId = VA_INVALID_ID;
    }

    if ( m_yuvSurfaceId != VA_INVALID_ID )
    {
        vaDestroySurfaces( m_display, &m_yuvSurfaceId, 1 );
        m_yuvSurfaceId = VA_INVALID_ID;
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
            size.width(), size.height(), &m_yuvSurfaceId, 1, &attrib, 1);

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaCreateContext( m_display, m_configId,
            size.width(), size.height(), VA_PROGRESSIVE, &m_yuvSurfaceId, 1, &m_contextId);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateContext:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaCreateBuffer( m_display, m_contextId,
            VAEncCodedBufferType, yuvSize( m_size ), 1, nullptr, &m_jpegBufferId);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateBuffer:" << vaErrorStr( vaStatus );
    }

}

bool VncVaEncoder::openDisplay()
{
    const auto paths =
    { "renderD128", "card0", "renderD129", "card1" };

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
        data.append( (const char*)segment->buf, segment->size );

    vaStatus = vaUnmapBuffer( m_display, bufferId );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaUnmapBuffer:" << vaErrorStr( vaStatus );

    return data;
}

QByteArray VncVaEncoder::encodeJPG( const QImage& image, const int quality )
{
    VAStatus vaStatus;

    setSize( image.size() );

    VASurfaceID bgra_surface;

    {
        VASurfaceAttrib attrib;
        attrib.type = VASurfaceAttribPixelFormat;
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        attrib.value.type = VAGenericValueTypeInteger;
        attrib.value.value.i = VA_FOURCC_BGRA; // why not VA_FOURCC_RGBA

        vaStatus = vaCreateSurfaces( m_display, VA_RT_FORMAT_RGB32,
            image.width(), image.height(), &bgra_surface, 1, &attrib, 1 );

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );

    }

    {
        VAImage vaImage;

        vaStatus = vaDeriveImage( m_display, bgra_surface, &vaImage );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaDeriveImage:" << vaErrorStr( vaStatus );

        uploadBGR( m_display, image.constBits(), vaImage );

        vaDestroyImage( m_display, vaImage.image_id );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaDestroyImage:" << vaErrorStr( vaStatus );
    }

    convertToNV12( m_display, image.size(), bgra_surface, m_yuvSurfaceId );

    vaDestroySurfaces( m_display, &bgra_surface, 1);

    m_encoder.initialize( m_display, m_contextId );
    m_encoder.encodeSurface( m_yuvSurfaceId, image.size(), quality, m_jpegBufferId );

    return bufferData( m_jpegBufferId );
}
