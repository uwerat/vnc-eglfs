/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaConverter.h"
#include "VncVa.h"
#include "VncEgl.h"

#include <qdebug.h>
#include <qrect.h>

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include <drm/drm_fourcc.h>

#include <fcntl.h>
#include <unistd.h>
#include <cstdint>

namespace
{
    class DmaBuffer : public VncEgl::DmaBuffer
    {
      public:
        DmaBuffer( unsigned int texture )
        {
            auto that = static_cast< VncEgl::DmaBuffer* >( this );
            *that = VncEgl::dmaBuffer( texture );
        }

        ~DmaBuffer()
        {
            if ( fd >= 0 )
                close( fd );
        }
    };
}

VncVaConverter::VncVaConverter()
{
    m_config = VncVa::createConfig( VAProfileNone, VAEntrypointVideoProc );
}

VncVaConverter::~VncVaConverter()
{
    VncVa::destroyBuffer( m_pipelineBuffer );
    VncVa::destroyConfig( m_config );
}

void VncVaConverter::updateContext( const QSize& size, VASurfaceID surface )
{
    m_surfaces[1] = surface;
    VncVa::destroyContext( m_context );
    m_context = VncVa::createContext( m_config, size, surface );
}

void VncVaConverter::setTexture(
    unsigned int texture, const QSize& textureSize, const QRect& subRect )
{
    VncVa::destroySurface( m_surfaces[0] );
    m_surfaces[0] = VA_INVALID_ID;

    DmaBuffer dma( texture );

    const auto sz = subRect.size();

    VASurfaceAttrib attribs[4] = {};

    auto attr = attribs;

    {
        attr->type = VASurfaceAttribUsageHint;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.type = VAGenericValueTypeInteger;
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

        desc.width = sz.width();
        desc.height = sz.height();

        desc.num_objects = 1;

        desc.objects[0].fd = dma.fd;
        desc.objects[0].drm_format_modifier = dma.modifier;
        desc.objects[0].size = dma.stride * textureSize.height();

        desc.num_layers = 1;

        auto& layer = desc.layers[0];
        layer.drm_format = DRM_FORMAT_RGBA8888;
        layer.num_planes = 1;
        layer.object_index[0] = 0;
        layer.pitch[0] = dma.stride;
        layer.offset[0] = dma.offset + subRect.y() * dma.stride + subRect.x() * 4;

        attr->type = VASurfaceAttribExternalBufferDescriptor;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.value.p = &desc;
        attr++;
    }

    m_surfaces[0] = VncVa::createSurface(
        VA_RT_FORMAT_RGB32, sz, attribs, attr - attribs  );
}

void VncVaConverter::updateParameters()
{
    VAProcPipelineParameterBuffer param = {};

    param.surface = m_surfaces[0];
    param.surface_region = nullptr;

    param.output_color_standard = VAProcColorStandardBT709;
    param.output_color_properties.color_range = VA_SOURCE_RANGE_FULL;

    param.rotation_state = VA_ROTATION_NONE; // VA_ROTATION_180
    param.mirror_state = VA_MIRROR_NONE; // VA_MIRROR_VERTICAL

    VncVa::destroyBuffer( m_pipelineBuffer );
    m_pipelineBuffer = VncVa::createBuffer( m_context,
        VAProcPipelineParameterBufferType, sizeof( param ), &param );
}

void VncVaConverter::run()
{
    VncVa::renderPicture( m_context, &m_pipelineBuffer, 1, m_surfaces[1] );
}
