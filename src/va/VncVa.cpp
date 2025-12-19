/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVa.h"
#include "VncEgl.h"

#include <qsize.h>
#include <qvarlengtharray.h>
#include <qdebug.h>

#include <va/va_drm.h>

#include <stdexcept>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>

namespace
{
    class Application
    {
      public:
        Application()
        {
            const auto path = VncEgl::devicePath();

            auto fd = open( path, O_RDWR | O_CLOEXEC );
            if ( fd < 0 )
            {
                qWarning() << "Can't open" << path << ":" << strerror( errno );
                return; // throw
            }

            auto display = vaGetDisplayDRM( fd );
            if ( display == nullptr )
            {
                close( fd );
                throw std::runtime_error( "vaGetDisplayDRM failed" );
            }

            int major_ver, minor_ver;
            auto status = vaInitialize( display, &major_ver, &minor_ver );
            if ( status != VA_STATUS_SUCCESS )
            {
                close( fd );
                VncVa::checkStatus( "vaInitialize", status );
            }

            m_display = display;
            m_drmFd = fd;
        }

        ~Application()
        {
            if ( m_display )
                vaTerminate( m_display );

            if ( m_drmFd >= 0 )
                close( m_drmFd );
        }

        VADisplay display() { return m_display; }

      private:
        int m_drmFd = -1;
        VADisplay m_display = nullptr;
    };
}

Q_GLOBAL_STATIC( Application, app );

void VncVa::checkStatus( const char* function, const VAStatus status )
{
    if ( status != VA_STATUS_SUCCESS )
    {
        QByteArray s = function;
        s += ": ";
        s += vaErrorStr( status );

        throw std::runtime_error( s.constData() );
    }
}

VADisplay VncVa::display()
{
    return app->display();
}

bool VncVa::hasEntryPoint( VAProfile profile, VAEntrypoint entry )
{
    QVarLengthArray< VAEntrypoint > entries( vaMaxNumEntrypoints( display() ) );

    int numEntries = 0;

    const auto vaStatus = vaQueryConfigEntrypoints(
        display(), profile, entries.data(), &numEntries );

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

VAConfigID VncVa::createConfig( VAProfile profile, VAEntrypoint entrypoint,
    const VAConfigAttrib* attributes, int count )
{
    VAConfigID config;

    const auto status = vaCreateConfig( display(), profile,
        entrypoint, const_cast< VAConfigAttrib* >( attributes ), count, &config );

    checkStatus( "vaCreateConfig", status );
    return config;
}

void VncVa::destroyConfig( VAConfigID config )
{
    if ( config != VA_INVALID_ID )
    {
        const auto status = vaDestroyConfig( display(), config );
        checkStatus( "vaDestroyConfig", status );
    }
}

VAContextID VncVa::createContext( VAConfigID config,
    const QSize& size, VASurfaceID surface )
{
    VAContextID context;

    const int count = ( surface != VA_INVALID_ID ) ? 1 : 0;

    const auto status = vaCreateContext( display(), config,
        size.width(), size.height(), VA_PROGRESSIVE, &surface, count, &context );
    checkStatus( "vaCreateContext", status );

    return context;
}

void VncVa::destroyContext( VAContextID context )
{
    if ( context != VA_INVALID_ID )
    {
        const auto status = vaDestroyContext( display(), context );
        checkStatus( "vaDestroyContext", status );
    }
}

VASurfaceID VncVa::createSurface( unsigned int format,
    const QSize& size, const VASurfaceAttrib* attributes, int count )
{
    VASurfaceID surface;

    const auto status = vaCreateSurfaces(
        display(), format, size.width(), size.height(), &surface, 1,
        const_cast< VASurfaceAttrib* >( attributes ), count );

    checkStatus( "vaCreateSurfaces", status );
    return surface;
}

void VncVa::destroySurface( VASurfaceID surface )
{
    if ( surface != VA_INVALID_ID )
    {
        const auto status = vaDestroySurfaces( display(), &surface, 1 );
        checkStatus( "vaDestroySurfaces", status );
    }
}

VABufferID VncVa::createBuffer( VAContextID context,
    VABufferType bufferType, unsigned int size, const void* data )
{
    VABufferID buffer;

    const auto status = vaCreateBuffer( display(), context,
        bufferType, size, 1, const_cast< void* >( data ), &buffer );
    checkStatus( "vaCreateBuffer", status );

    return buffer;
}

void VncVa::destroyBuffer( VABufferID buffer )
{
    if ( buffer != VA_INVALID_ID )
    {
        const auto status = vaDestroyBuffer( display(), buffer );
        checkStatus( "vaDestroyBuffer", status );
    }
}

QByteArray VncVa::bufferData( VABufferID buffer )
{
    VACodedBufferSegment* segment;

    auto vaStatus = vaMapBuffer( display(), buffer, (void**)( &segment ) );
    checkStatus( "vaMapBuffer", vaStatus );

    QByteArray data;
    if ( !( segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK ) )
    {
        data.resize( segment->size );
        memcpy( data.data(), segment->buf, segment->size );
    }

    vaStatus = vaUnmapBuffer( display(), buffer );
    checkStatus( "vaUnmapBuffer", vaStatus );

    return data;
}

void VncVa::renderPicture( VAContextID context,
    const VABufferID* buffers, int count, VASurfaceID surface )
{
    auto vaStatus = vaBeginPicture( display(), context, surface );
    checkStatus( "vaBeginPicture", vaStatus );

    vaStatus = vaRenderPicture( display(), context,
        const_cast< VABufferID* >( buffers ), count );
    checkStatus( "vaRenderPicture", vaStatus );

    vaStatus = vaEndPicture( display(), context );
    checkStatus( "vaEndPicture:", vaStatus );

    vaStatus = vaSyncSurface( display(), surface );
    checkStatus( "vaSyncSurface:", vaStatus );
}
