/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaApplication.h"
#include "VncVaConverter.h"
#include "VncVaEncoder.h"

#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>

#include <qdebug.h>
#include <qvarlengtharray.h>

#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include <drm/drm_fourcc.h>

static bool hasConfig( VADisplay display, VAProfile profile, VAEntrypoint entry )
{
    QVarLengthArray< VAEntrypoint > entries( vaMaxNumEntrypoints( display ) );

    int numEntries = 0;

    const auto vaStatus = vaQueryConfigEntrypoints(
        display, profile, entries.data(), &numEntries );

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

VncVaApplication::VncVaApplication()
{
}

VncVaApplication::~VncVaApplication()
{
    close();
}

bool VncVaApplication::isValid()
{
    VncVaApplication encoder;
    encoder.openDisplay();

    return hasConfig( encoder.m_display, VAProfileNone, VAEntrypointVideoProc )
        && hasConfig( encoder.m_display, VAProfileJPEGBaseline, VAEntrypointEncPicture );
}

bool VncVaApplication::open()
{
    if ( m_display )
        return true;

    openDisplay();

    m_converter = new VncVaConverter( m_display );
    m_encoder = new VncVaEncoder( m_display );

    return true;
}

void VncVaApplication::close()
{
    delete m_converter;
    m_converter = nullptr;

    delete m_encoder;
    m_encoder = nullptr;

    closeDisplay();
}

bool VncVaApplication::openDisplay()
{
    // using Qt - see gbm TODO ...
    const auto paths = { "renderD128", "card0", "renderD129", "card1" };

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

void VncVaApplication::closeDisplay()
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

QByteArray VncVaApplication::encode( unsigned int texture, const QSize& textureSize,
    const QRect& subRect, int quality )
{
    const auto size = subRect.size();

    if ( size != m_encoder->surfaceSize() )
    {
        m_encoder->resizeSurface( size );

        m_encoder->updateContext( size, m_encoder->surface() );
        m_converter->updateContext( size, m_encoder->surface() );
    }

    m_converter->setTexture( texture, textureSize, subRect );
    m_converter->updateParameters();
    m_converter->run();

    m_encoder->updateParameters( quality );
    m_encoder->run();

    return m_encoder->encodedData();
}
