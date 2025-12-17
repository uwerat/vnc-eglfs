/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaRenderer.h"

#include <qdebug.h>
#include <qvarlengtharray.h>
#include <qbytearray.h>
#include <qsize.h>

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

VncVaRenderer::VncVaRenderer( VADisplay display )
    : m_display( display )
{
}

VncVaRenderer::~VncVaRenderer()
{
    for ( auto buffer : m_parameterBuffers )
        destroyBuffer( buffer );

    if ( m_config != VA_INVALID_ID )
    {
        vaDestroyConfig( m_display, m_config );
        m_config = VA_INVALID_ID;
    }
}

void VncVaRenderer::createConfig( VAProfile profile, VAEntrypoint entrypoint,
    const VAConfigAttrib* attributes, int count )
{
    auto vaStatus = vaCreateConfig( m_display, profile,
        entrypoint, const_cast< VAConfigAttrib* >( attributes ), count, &m_config );

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateConfig:" << vaErrorStr( vaStatus );
}

void VncVaRenderer::destroyContext()
{
    if ( m_context != VA_INVALID_ID )
    {
        vaDestroyContext( m_display, m_context );
        m_context = VA_INVALID_ID;
    }
}

VASurfaceID VncVaRenderer::createSurface( unsigned int format,
    const QSize& size, const VASurfaceAttrib* attributes, int count )
{
    VASurfaceID surface;

    auto vaStatus = vaCreateSurfaces( m_display, format,
        size.width(), size.height(), &surface, 1,
        const_cast< VASurfaceAttrib* >( attributes ), count  );

    if( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateSurfaces:" << vaErrorStr( vaStatus );

    return surface;
}

void VncVaRenderer::destroySurface( VASurfaceID surface )
{
    if ( surface != VA_INVALID_ID )
        vaDestroySurfaces( m_display, &surface, 1 );

}

VABufferID VncVaRenderer::createBuffer(
    VABufferType type, unsigned int size, void* data )
{
    VABufferID buffer;

    auto vaStatus = vaCreateBuffer( m_display, m_context,
        type, size, 1, data, &buffer);

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateBuffer:" << vaErrorStr( vaStatus );

    return buffer;
}

void VncVaRenderer::destroyBuffer( VABufferID buffer )
{
    if ( buffer != VA_INVALID_ID )
        vaDestroyBuffer( m_display, buffer );
}

void VncVaRenderer::setParameterCount( int count )
{
    m_parameterBuffers.reserve( count );
    m_parameterBuffers.resize( count, VA_INVALID_ID );
}

void VncVaRenderer::setParameterBuffer( int index,
    VABufferType bufferType, unsigned int size, const void* data )
{
    destroyBuffer( m_parameterBuffers[index] );

    VABufferID buffer = VA_INVALID_ID;

    const auto vaStatus = vaCreateBuffer( m_display, m_context,
        bufferType, size, 1, const_cast< void* >( data ), &buffer );

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateBuffer:" << bufferType << vaErrorStr( vaStatus );

    m_parameterBuffers[index] = buffer;
}

void VncVaRenderer::createContext( const QSize& size, VASurfaceID surface )
{
    auto vaStatus = vaCreateContext( m_display, m_config,
        size.width(), size.height(), VA_PROGRESSIVE, &surface, 1, &m_context );

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateContext:" << vaErrorStr( vaStatus );
}

void VncVaRenderer::render( VASurfaceID surface )
{
    const auto& buffers = m_parameterBuffers;

    auto vaStatus = vaBeginPicture( m_display, m_context, surface );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

    vaStatus = vaRenderPicture( m_display, m_context,
        const_cast< VABufferID* >( buffers.constData() ), buffers.size() );

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

    vaStatus = vaEndPicture( m_display, m_context );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaEndPicture:" << vaErrorStr( vaStatus );

    vaStatus = vaSyncSurface( m_display, surface );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );
}

QByteArray VncVaRenderer::bufferData( VABufferID buffer ) const
{
    VACodedBufferSegment* segment;

    auto vaStatus = vaMapBuffer( m_display, buffer, (void**)( &segment ) );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaMapBuffer:" << vaErrorStr( vaStatus );

    QByteArray data;
    if ( !( segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK ) )
    {
        data.resize( segment->size );
        memcpy( data.data(), segment->buf, segment->size );
    }

    vaStatus = vaUnmapBuffer( m_display, buffer );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaUnmapBuffer:" << vaErrorStr( vaStatus );

    return data;
}
