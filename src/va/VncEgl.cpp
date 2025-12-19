/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncEgl.h"

#include <qglobal.h>
#include <qdebug.h>
#include <qstring.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace
{
    // EXT
    auto eglQueryDeviceStringEXT =
        (PFNEGLQUERYDEVICESTRINGEXTPROC) eglGetProcAddress("eglQueryDeviceStringEXT");

    // KHR
    auto eglQueryDisplayAttribKHR =
        (PFNEGLQUERYDISPLAYATTRIBKHRPROC)eglGetProcAddress("eglQueryDisplayAttribKHR");

    auto eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");

    auto eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");

    // MESA
    auto eglExportDMABUFImageMESA =
        (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");

    auto eglExportDMABUFImageQueryMESA =
        (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC) eglGetProcAddress("eglExportDMABUFImageQueryMESA");
}

namespace
{
    #define ERROR_STR( value ) case value: return #value;

    const char* getEglErrorString()
    {
        switch( eglGetError() )
        {
            ERROR_STR( EGL_SUCCESS             )
            ERROR_STR( EGL_NOT_INITIALIZED     )
            ERROR_STR( EGL_BAD_ACCESS          )
            ERROR_STR( EGL_BAD_ALLOC           )
            ERROR_STR( EGL_BAD_ATTRIBUTE       )
            ERROR_STR( EGL_BAD_CONTEXT         )
            ERROR_STR( EGL_BAD_CONFIG          )
            ERROR_STR( EGL_BAD_CURRENT_SURFACE )
            ERROR_STR( EGL_BAD_DISPLAY         )
            ERROR_STR( EGL_BAD_SURFACE         )
            ERROR_STR( EGL_BAD_MATCH           )
            ERROR_STR( EGL_BAD_PARAMETER       )
            ERROR_STR( EGL_BAD_NATIVE_PIXMAP   )
            ERROR_STR( EGL_BAD_NATIVE_WINDOW   )
            ERROR_STR( EGL_CONTEXT_LOST        )
            default: return "Unknown";
        }
    }

    #undef CASE_STR

    EGLDeviceEXT deviceEgl( EGLDisplay display )
    {
        EGLAttrib attr;

        if ( !eglQueryDisplayAttribKHR( display, EGL_DEVICE_EXT, &attr) )
        {
            qDebug() << "eglQueryDisplayAttribKHR:" << getEglErrorString();
            return nullptr;
        }

        return reinterpret_cast< EGLDeviceEXT >( attr );
    }

    bool checkFeatures()
    {
        if ( eglGetCurrentDisplay() )
        {
            return eglCreateImageKHR && eglDestroyImageKHR
                && eglExportDMABUFImageMESA && eglExportDMABUFImageQueryMESA;
        }

        return false;
    }
}

bool VncEgl::isSufficient()
{
    static int sufficient = -1;

    if ( sufficient < 0 )
        sufficient = checkFeatures();

    return sufficient;
}

const char* VncEgl::devicePath()
{
    const char* path = nullptr;

    if ( eglQueryDisplayAttribKHR && eglQueryDeviceStringEXT )
    {
        /*
            we might have more than one GPU and need to find the one where
            the textures are. Otherwise we can't process them without
            downloading to the CPU.
         */
        auto display = eglGetCurrentDisplay();
        Q_ASSERT( display );

        auto device = deviceEgl( display );
        if ( device )
            path = eglQueryDeviceStringEXT( device, EGL_DRM_RENDER_NODE_FILE_EXT );
    }

    if ( path == nullptr )
        path = "/dev/dri/renderD128";

    return path;
}

VncEgl::DmaBuffer VncEgl::dmaBuffer( unsigned int texture )
{
    DmaBuffer buf;

    Q_ASSERT( eglCreateImageKHR && eglDestroyImageKHR );
    Q_ASSERT( eglExportDMABUFImageMESA && eglExportDMABUFImageQueryMESA );

    auto eglDisplay = eglGetCurrentDisplay();
    auto eglContext = eglGetCurrentContext();

    Q_ASSERT( eglDisplay && eglContext != EGL_NO_CONTEXT );

    const EGLint attrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_FALSE, EGL_NONE };

    auto image = eglCreateImageKHR( eglDisplay, eglContext,
        EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)(uintptr_t)texture, attrs);

    if ( image == EGL_NO_IMAGE_KHR )
    {
        qWarning() << "eglCreateImageKHR failed:" << texture << getEglErrorString();
        return buf;
    }

    int fds[4] = { -1, -1, -1, -1 };
    int strides[4] = {};
    int offsets[4] = {};
    int fourcc[4] = {};
    int num_planes[4] = {};
    EGLuint64KHR modifiers[4] = {};

    auto ok = eglExportDMABUFImageMESA( eglDisplay, image, fds, strides, offsets );
    if ( ok )
    {
        Q_ASSERT( fds[1] == -1 );

        ok = eglExportDMABUFImageQueryMESA( eglDisplay, image,
            fourcc, num_planes, modifiers );

        if ( !ok )
            qWarning() << "eglExportDMABUFImageMESA failed:" << getEglErrorString();
    }
    else
    {
        qWarning() << "eglExportDMABUFImageMESA failed:" << getEglErrorString();
    }

    if ( ok )
    {
        buf.fd = fds[0];
        buf.stride = strides[0];
        buf.offset = offsets[0];

        buf.modifier = modifiers[0];
        buf.fourcc = fourcc[0];
    }

    eglDestroyImageKHR( eglDisplay, image );
    return buf;
}
