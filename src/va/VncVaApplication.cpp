/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaApplication.h"
#include "VncVaConverter.h"
#include "VncVaEncoder.h"
#include "VncVa.h"

#include <qdebug.h>

VncVaApplication::VncVaApplication()
{
}

VncVaApplication::~VncVaApplication()
{
}

bool VncVaApplication::isValid()
{
    return VncVa::hasEntryPoint( VAProfileNone, VAEntrypointVideoProc )
        && VncVa::hasEntryPoint( VAProfileJPEGBaseline, VAEntrypointEncPicture );
}

void VncVaApplication::open()
{
    m_converter = new VncVaConverter();
    m_encoder = new VncVaEncoder();
}

void VncVaApplication::close()
{
    delete m_converter;
    m_converter = nullptr;

    delete m_encoder;
    m_encoder = nullptr;
}

QByteArray VncVaApplication::encode( unsigned int texture,
    const QSize& textureSize, const QRect& subRect, int quality )
{
    QByteArray data;

    try
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

        data = m_encoder->encodedData();
    }
    catch( const std::exception& e )
    {
        qWarning() << e.what();
    }

    return data;
}
