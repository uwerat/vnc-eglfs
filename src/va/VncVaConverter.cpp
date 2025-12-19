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

VncVaConverter::VncVaConverter()
{
    m_config = VncVa::createConfig( VAProfileNone, VAEntrypointVideoProc );
}

VncVaConverter::~VncVaConverter()
{
    VncVa::destroyBuffer( m_pipelineBuffer );
    VncVa::destroySurface( m_textureSurface );
    VncVa::destroyConfig( m_config );
}

void VncVaConverter::updateContext( const QSize& size )
{
    VncVa::destroyContext( m_context );
    m_context = VncVa::createContext( m_config, size );
}

void VncVaConverter::setSource( VncEgl::DmaBuffer& dma,
    const QSize& sourceSize, const QRect& subRect )
{
    VncVa::destroySurface( m_textureSurface );
    m_textureSurface = VA_INVALID_ID;

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

#if 1
    {
        attr->type = VASurfaceAttribMemoryType;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.type = VAGenericValueTypeInteger;
        attr->value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2;
        attr++;
    }
#endif
    {

        VADRMPRIMESurfaceDescriptor desc = {};

        desc.fourcc = VA_FOURCC_RGBA;

        desc.width = sz.width();
        desc.height = sz.height();

        desc.num_objects = 1;

        desc.objects[0].fd = dma.fd;
        desc.objects[0].drm_format_modifier = dma.modifier;
        desc.objects[0].size = dma.stride * sourceSize.height();

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

    m_textureSurface = VncVa::createSurface(
        VA_RT_FORMAT_RGB32, sz, attribs, attr - attribs  );

    updatePipeline( m_textureSurface );
}

void VncVaConverter::updatePipeline( VASurfaceID surface )
{
    VAProcPipelineParameterBuffer param = {};

    param.surface = surface;
    param.surface_region = nullptr;

    param.output_color_standard = VAProcColorStandardBT709;
    param.output_color_properties.color_range = VA_SOURCE_RANGE_FULL;

    param.rotation_state = VA_ROTATION_NONE; // VA_ROTATION_180
    param.mirror_state = VA_MIRROR_NONE; // VA_MIRROR_VERTICAL

    VncVa::destroyBuffer( m_pipelineBuffer );
    m_pipelineBuffer = VncVa::createBuffer( m_context,
        VAProcPipelineParameterBufferType, sizeof( param ), &param );
}

void VncVaConverter::render( VASurfaceID surface )
{
    VncVa::renderPicture( m_context, &m_pipelineBuffer, 1, surface );
}
