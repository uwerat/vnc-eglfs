/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncVaApplication.h"
#include "VncVaConverter.h"
#include "VncVaEncoder.h"
#include "VncVa.h"
#include "VncEgl.h"

#include <qrect.h>
#include <qdebug.h>

#include <unistd.h>

class VncVaApplication::Encoder
{
  public:
    Encoder( unsigned int );
    ~Encoder();

    void update( const QSize& sourceSize, const QRect& subRect, int quality );
    QByteArray encode();

  private:
    VncVaConverter m_converter;
    VncVaEncoder m_encoder;

    VASurfaceID m_surface = VA_INVALID_ID;

    const unsigned int m_sourceTexture;

    QSize m_sourceSize;
    QRect m_subRect;
    int m_quality = 0;

    VncEgl::DmaBuffer m_dmaBuffer;
};

VncVaApplication::Encoder::Encoder( unsigned int texture )
    : m_sourceTexture( texture )
{
}

VncVaApplication::Encoder::~Encoder()
{
    VncVa::destroySurface( m_surface );

    if ( m_dmaBuffer.fd >= 0 )
        ::close( m_dmaBuffer.fd );
}

void VncVaApplication::Encoder::update(
    const QSize& sourceSize, const QRect& subRect, int quality )
{
    if ( sourceSize != m_sourceSize )
    {
        if ( m_dmaBuffer.fd >= 0 )
            ::close( m_dmaBuffer.fd );

        // strides depend on the texture size
        m_dmaBuffer = VncEgl::dmaBuffer( m_sourceTexture );

        m_encoder.updateContext( sourceSize );
        m_converter.updateContext( sourceSize );
    }

    if ( subRect.size() != m_subRect.size() )
    {
        VncVa::destroySurface( m_surface );
        m_surface = VncVa::createSurface( VA_RT_FORMAT_YUV420, subRect.size() );
    }

    if ( subRect.size() != m_subRect.size() || quality != m_quality )
        m_encoder.updateParameters( subRect.size(), quality );

    if ( sourceSize != m_sourceSize || subRect != m_subRect )
        m_converter.setSource( m_dmaBuffer, sourceSize, subRect );

    m_sourceSize = sourceSize;
    m_subRect = subRect;
    m_quality = quality;
}

QByteArray VncVaApplication::Encoder::encode()
{
    m_converter.render( m_surface );
    m_encoder.render( m_surface );

    return m_encoder.encodedData();
}

VncVaApplication::VncVaApplication( unsigned int texture )
    : m_encoder( new Encoder( texture ) )
{
}

VncVaApplication::~VncVaApplication()
{
    delete m_encoder;
}

bool VncVaApplication::isValid()
{
    return VncVa::hasEntryPoint( VAProfileNone, VAEntrypointVideoProc )
        && VncVa::hasEntryPoint( VAProfileJPEGBaseline, VAEntrypointEncPicture );
}

QByteArray VncVaApplication::encode(
    const QSize& sourceSize, const QRect& subRect, int quality )
{
    QByteArray data;

    try
    {
        m_encoder->update( sourceSize, subRect, quality );
        data = m_encoder->encode();
    }
    catch( const std::exception& e )
    {
        qWarning() << e.what();
    }

    return data;
}
