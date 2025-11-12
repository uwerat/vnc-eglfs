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
#include <va/va_drm.h>

static inline uint8_t clampToByte(float v)
{
    const int b = v + 0.5f;
    return ( b < 0 ) ? 0 : ( b > 255 ) ? 255 : b;
}

static inline int yuvSize( const QSize& size )
{
    const auto w = size.width();
    const auto h = size.height();

    return w * h + 2 * ceil( 0.5 * w ) * ceil( 0.5 * h );
}

void rgbaToNV12( const uint8_t* rgba, int width, int height, uint8_t* nv12 )
{
    uint8_t* y_plane = nv12;
    uint8_t* uv_plane = nv12 + width * height;

    // --- Luma (Y) plane ---
    for ( int y = 0; y < height; y++ )
    {
        for (int x = 0; x < width; x++)
        {
            const int idx = ( y * width + x ) * 4;

            const auto b = rgba[idx + 0];
            const auto g = rgba[idx + 1];
            const auto r = rgba[idx + 2];

            // BT.709 luma conversion
            const float Yf = 0.2126f * r + 0.7152f * g + 0.0722f * b;

            y_plane[y * width + x] = clampToByte(Yf);
        }
    }

    // --- Chroma (UV) plane ---
    for ( int y = 0; y < height; y += 2 )
    {
        const auto uv = uv_plane + ( y / 2 ) * width;

        for ( int x = 0; x < width; x += 2 )
        {
            float Usum = 0.0f, Vsum = 0.0f;

            // Average over 2x2 block
            for ( int j = 0; j < 2; ++j )
            {
                for ( int i = 0; i < 2; ++i )
                {
                    const int sx = std::min(x + i, width - 1);
                    const int sy = std::min(y + j, height - 1);

                    const int idx = ( sy * width + sx ) * 4;

                    const auto b = rgba[idx + 0];
                    const auto g = rgba[idx + 1];
                    const auto r = rgba[idx + 2];

                    // BT.709 chroma
                    Usum += ( -0.1146f * r - 0.3854f * g + 0.5000f * b + 128.0f );
                    Vsum += ( 0.5000f * r - 0.4542f * g - 0.0458f * b + 128.0f );
                }
            }

            uv[x + 0] = clampToByte( Usum / 4.0f );
            uv[x + 1] = clampToByte( Vsum / 4.0f );
        }
    }
}

static inline QVector< uint8_t > imageToNV12( const QImage& image )
{
    QVector< uint8_t > bytes( yuvSize( image.size() ) );
    const uint8_t* rgb = reinterpret_cast< const uint8_t* >( image.constBits() );

    rgbaToNV12( rgb, image.width(), image.height(), bytes.data() );

    return bytes;
}

static void uploadNV12( VADisplay vaDisplay,
    const uint8_t* nv12, VAImage& vaImage )
{
    uint8_t* vaBuffer = NULL;
    vaMapBuffer( vaDisplay, vaImage.buf, reinterpret_cast< void** >( &vaBuffer ) );

    {
        const auto width = vaImage.width;
        const auto height = vaImage.height;

        const auto yPlane = nv12;
        const auto uvPlane = nv12 + width * height;

        const auto yPlaneVA = vaBuffer + vaImage.offsets[0];
        const auto uvPlaneVA = vaBuffer + vaImage.offsets[1];

        for ( int row = 0; row < height; row++)
        {
            memcpy( yPlaneVA + row * vaImage.pitches[0],
                yPlane + row * width, width );
        }

        for ( int row = 0; row < height / 2; row++)
        {
            memcpy( uvPlaneVA + row * vaImage.pitches[1],
                uvPlane + row * width, width );
        }
    }

    vaUnmapBuffer( vaDisplay, vaImage.buf );
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

    if ( m_targetId != VA_INVALID_ID )
    {
        vaDestroyBuffer( m_display, m_targetId );
        m_targetId = VA_INVALID_ID;
    }

    if ( m_contextId != VA_INVALID_ID )
    {
        vaDestroyContext( m_display, m_contextId );
        m_contextId = VA_INVALID_ID;
    }

    if ( m_surfaceId != VA_INVALID_ID )
    {
        vaDestroySurfaces( m_display, &m_surfaceId, 1 );
        m_surfaceId = VA_INVALID_ID;
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
            size.width(), size.height(), &m_surfaceId, 1, &attrib, 1);

        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaCreateContext( m_display, m_configId,
            size.width(), size.height(), VA_PROGRESSIVE, &m_surfaceId, 1, &m_contextId);
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaCreateContext:" << vaErrorStr( vaStatus );
    }

    {
        auto vaStatus = vaCreateBuffer( m_display, m_contextId,
            VAEncCodedBufferType, yuvSize( m_size ), 1, nullptr, &m_targetId);
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

QByteArray VncVaEncoder::targetBufferData()
{
    VACodedBufferSegment* segment;

    auto vaStatus = vaMapBuffer( m_display, m_targetId, (void**)( &segment ) );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaMapBuffer:" << vaErrorStr( vaStatus );

    QByteArray data;
    if ( !( segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK ) )
        data.append( (const char*)segment->buf, segment->size );

    vaStatus = vaUnmapBuffer( m_display, m_targetId );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaUnmapBuffer:" << vaErrorStr( vaStatus );

    return data;
}

QByteArray VncVaEncoder::encodeJPG( const QImage& image, const int quality )
{
    setSize( image.size() );

    const auto buffer = imageToNV12( image );

    {
        VAImage vaImage;

        auto vaStatus = vaDeriveImage( m_display, m_surfaceId, &vaImage );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaDeriveImage:" << vaErrorStr( vaStatus );

        uploadNV12( m_display, buffer.constData(), vaImage );

        vaDestroyImage( m_display, vaImage.image_id );
        if ( vaStatus != VA_STATUS_SUCCESS )
            qWarning() << "vaDestroyImage:" << vaErrorStr( vaStatus );
    }

    m_encoder.initialize( m_display, m_contextId );
    m_encoder.encodeSurface( m_surfaceId, image.size(), quality, m_targetId );

    return targetBufferData();
}
