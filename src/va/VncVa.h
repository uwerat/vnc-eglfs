/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <va/va.h>
#include <qbytearray.h>

class QSize;

namespace VncVa
{
    void checkStatus( const char* function, VAStatus );
    bool hasEntryPoint( VAProfile, VAEntrypoint );

    VADisplay display();

    // config
    VAConfigID createConfig( VAProfile, VAEntrypoint,
        const VAConfigAttrib* = nullptr, int count = 0 );
    void destroyConfig( VAConfigID );

    // context
    VAContextID createContext( VAConfigID, const QSize&, VASurfaceID );
    void destroyContext( VAContextID );

    // surface
    VASurfaceID createSurface( unsigned int format,
        const QSize&, const VASurfaceAttrib*, int count );
    void destroySurface( VASurfaceID );

    // buffer
    VABufferID createBuffer( VAContextID, VABufferType,
        unsigned int size, const void* = nullptr );

    void destroyBuffer( VABufferID );

    QByteArray bufferData( VABufferID );

    // picture
    void renderPicture( VAContextID, const VABufferID*, int count, VASurfaceID );
}
