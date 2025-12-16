/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaConverterPass.h"

#include <qdebug.h>
#include <qrect.h>

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include <drm/drm_fourcc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <fcntl.h>
#include <unistd.h>
#include <cstdint>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <drm/drm_fourcc.h>

#include <qsize.h>
#include <qdebug.h>

namespace
{
    #define CASE_STR( value ) case value: return #value;

    const char* getEglErrorString()
    {
        switch( eglGetError() )
        {
            CASE_STR( EGL_SUCCESS             )
            CASE_STR( EGL_NOT_INITIALIZED     )
            CASE_STR( EGL_BAD_ACCESS          )
            CASE_STR( EGL_BAD_ALLOC           )
            CASE_STR( EGL_BAD_ATTRIBUTE       )
            CASE_STR( EGL_BAD_CONTEXT         )
            CASE_STR( EGL_BAD_CONFIG          )
            CASE_STR( EGL_BAD_CURRENT_SURFACE )
            CASE_STR( EGL_BAD_DISPLAY         )
            CASE_STR( EGL_BAD_SURFACE         )
            CASE_STR( EGL_BAD_MATCH           )
            CASE_STR( EGL_BAD_PARAMETER       )
            CASE_STR( EGL_BAD_NATIVE_PIXMAP   )
            CASE_STR( EGL_BAD_NATIVE_WINDOW   )
            CASE_STR( EGL_CONTEXT_LOST        )
            default: return "Unknown";
        }
    }

    #undef CASE_STR

    auto eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");

    auto eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");

    auto eglExportDMABUFImageMESA =
        (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");

    auto eglExportDMABUFImageQueryMESA =
        (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC) eglGetProcAddress("eglExportDMABUFImageQueryMESA");

}

namespace
{
    class DmaBuffer
    {
      public:
        DmaBuffer( unsigned int );
        ~DmaBuffer();

        static bool isSupported();

        int fd() const { return m_fd; }
        int stride() const { return m_stride; }
        int offset() const { return m_offset; }
        uint64_t modifier() const { return m_modifier; }
        int fourcc() const { return m_fourcc; }

      private:
        bool setTexture( unsigned int );

        void* m_image = nullptr;

        int m_fd = -1;
        int m_stride = 0;
        int m_offset = 0;

        uint64_t m_modifier = 0;
        int m_fourcc = 0;
    };

    bool DmaBuffer::isSupported()
    {
        if ( eglCreateImageKHR && eglDestroyImageKHR
             && eglExportDMABUFImageMESA && eglExportDMABUFImageQueryMESA )
        {
            return eglGetCurrentDisplay() && eglGetCurrentContext();
        }

        return false;
    }

    DmaBuffer::DmaBuffer( unsigned int textureId )
    {
        setTexture( textureId );
    }

    DmaBuffer::~DmaBuffer()
    {
        if ( m_image != EGL_NO_IMAGE_KHR )
        {
            eglDestroyImageKHR( eglGetCurrentDisplay(), m_image );
            m_image = EGL_NO_IMAGE_KHR;
        }

        if ( m_fd >= 0 )
            ::close( m_fd );
    }

    bool DmaBuffer::setTexture( unsigned int textureId )
    {
        Q_ASSERT( eglCreateImageKHR && eglExportDMABUFImageMESA );

        auto eglDisplay = eglGetCurrentDisplay();
        auto eglContext = eglGetCurrentContext();

        Q_ASSERT( eglDisplay && eglContext != EGL_NO_CONTEXT );

        EGLint attrs[] = { EGL_NONE };

        m_image = eglCreateImageKHR( eglDisplay, eglContext,
            EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)(uintptr_t)textureId, attrs);

        if ( m_image == EGL_NO_IMAGE_KHR )
        {
            qWarning() << "eglCreateImageKHR failed:" << textureId << getEglErrorString();
            return false;
        }

        int fds[4] = { -1, -1, -1, -1 };
        int strides[4], offsets[4];

        EGLBoolean ok = eglExportDMABUFImageMESA(
            eglDisplay, m_image, fds, strides, offsets );

        if (!ok)
        {
            qWarning() << "eglExportDMABUFImageMESA failed:" << getEglErrorString();
            return false;
        }

        Q_ASSERT( fds[1] == -1 );

        m_fd = fds[0];
        m_stride = strides[0];
        m_offset = offsets[0];

        int fourcc[4] = {};
        int num_planes[4] = {};
        EGLuint64KHR modifiers[4] = {};

        ok = eglExportDMABUFImageQueryMESA( eglDisplay, m_image,
            fourcc, num_planes, modifiers );
        if ( ok )
        {
            m_modifier = modifiers[0];
            m_fourcc = fourcc[0];
        }

        return true;
    }
}

VncVaConverterPass::VncVaConverterPass( VADisplay display )
    : VncVaRenderPass( display )
{
    Q_ASSERT( DmaBuffer::isSupported() );
    createConfig();
}

VncVaConverterPass::~VncVaConverterPass()
{
}

void VncVaConverterPass::createConfig()
{
    auto vaStatus = vaCreateConfig( m_display, VAProfileNone,
        VAEntrypointVideoProc, nullptr, 0, &m_config );

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateConfig:" << vaErrorStr( vaStatus );
}

void VncVaConverterPass::createSurface(
    unsigned int texture, const QSize& textureSize, const QRect& subRect )
{
    DmaBuffer dma( texture );

    if ( m_surface != VA_INVALID_ID )
        vaDestroySurfaces( m_display, &m_surface, 1 );

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

        desc.objects[0].fd = dma.fd();
        desc.objects[0].drm_format_modifier = dma.modifier();
        desc.objects[0].size = dma.stride() * textureSize.height();

        desc.num_layers = 1;

        auto& layer = desc.layers[0];
        layer.drm_format = DRM_FORMAT_RGBA8888;
        layer.num_planes = 1;
        layer.object_index[0] = 0;
        layer.pitch[0] = dma.stride();
        layer.offset[0] = dma.offset() + subRect.y() * dma.stride() + subRect.x() * 4;

        attr->type = VASurfaceAttribExternalBufferDescriptor;
        attr->flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr->value.value.p = &desc;
        attr++;
    }

    const auto count = attr - attribs;

    auto vaStatus = vaCreateSurfaces(
        m_display, VA_RT_FORMAT_RGB32, sz.width(), sz.height(),
        &m_surface, 1, attribs, count  );

    if( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );
}

void VncVaConverterPass::updateBuffers()
{
    VAProcPipelineParameterBuffer params = {};

    params.surface = m_surface;

    params.surface_region = nullptr;

    params.output_color_standard = VAProcColorStandardBT709;
    params.output_color_properties.color_range = VA_SOURCE_RANGE_FULL;

    params.rotation_state = VA_ROTATION_NONE; // VA_ROTATION_180
    params.mirror_state = VA_MIRROR_NONE; // VA_MIRROR_VERTICAL

    setBuffer( VAProcPipelineParameterBufferType, sizeof( params ), &params );
}
