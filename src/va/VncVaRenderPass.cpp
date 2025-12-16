/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaRenderPass.h"

#include <qdebug.h>
#include <qvarlengtharray.h>
#include <qsize.h>

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

VncVaRenderPass::VncVaRenderPass( VADisplay display )
    : m_display( display )
{
}

VncVaRenderPass::~VncVaRenderPass()
{
    close();
}

void VncVaRenderPass::destroyContext()
{
    if ( m_context != VA_INVALID_ID )
    {
        vaDestroyContext( m_display, m_context );
        m_context = VA_INVALID_ID;
    }

    if ( m_surface != VA_INVALID_ID )
    {
        vaDestroySurfaces( m_display, &m_surface, 1 );
        m_surface = VA_INVALID_ID;
    }
}

void VncVaRenderPass::setBuffer(
    VABufferType bufferType, unsigned int size, const void* data )
{
    VABufferID buffer = VA_INVALID_ID;

    const auto vaStatus = vaCreateBuffer( m_display, m_context,
        bufferType, size, 1, const_cast< void* >( data ), &buffer );

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateBuffer:" << bufferType << vaErrorStr( vaStatus );

    auto it = m_bufferMap.find( bufferType );
    if ( it != m_bufferMap.end() )
    {
        vaDestroyBuffer( m_display, it.value() );
        it.value() = buffer;
    }
    else
    {
        m_bufferMap.insert( bufferType, buffer );
    }
}

void VncVaRenderPass::destroyBuffers()
{
    for ( auto& buffer : m_bufferMap )
    {
        if ( buffer != VA_INVALID_ID )
        {
            vaDestroyBuffer( m_display, buffer );
            buffer = VA_INVALID_ID;
        }
    }
    m_bufferMap.clear();
}

void VncVaRenderPass::destroyBuffer( VABufferType bufferType )
{
    auto it = m_bufferMap.find( bufferType );
    if ( it != m_bufferMap.end() )
    {
        if ( it.value() != VA_INVALID_ID )
            vaDestroyBuffer( m_display, it.value() );

        m_bufferMap.erase( it );
    }
}

void VncVaRenderPass::close()
{
    destroyBuffers();

    if ( m_config != VA_INVALID_ID )
    {
        vaDestroyConfig( m_display, m_config );
        m_config = VA_INVALID_ID;
    }
}

void VncVaRenderPass::createContext( const QSize& size, VASurfaceID surface )
{
    m_renderSurface = surface;

    auto vaStatus = vaCreateContext( m_display, m_config,
        size.width(), size.height(), VA_PROGRESSIVE, &surface, 1, &m_context );

    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaCreateContext:" << vaErrorStr( vaStatus );
}

void VncVaRenderPass::run()
{
    QVarLengthArray< VABufferID > array;
    array.reserve( m_bufferMap.size() );

    for ( auto buf : std::as_const( m_bufferMap ) )
        array += buf;

    auto vaStatus = vaBeginPicture( m_display, m_context, m_renderSurface );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

    vaStatus = vaRenderPicture( m_display, m_context, array.data(), array.size() );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );

    vaStatus = vaEndPicture( m_display, m_context );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaEndPicture:" << vaErrorStr( vaStatus );

    vaStatus = vaSyncSurface( m_display, m_renderSurface );
    if ( vaStatus != VA_STATUS_SUCCESS )
        qWarning() << "vaBeginPicture:" << vaErrorStr( vaStatus );
}
