/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "VncFrame.h"

VncFrame::VncFrame()
{
}

VncFrame::VncFrame( Encoding encoding, const QRect& region, const QByteArray& bytes )
    : m_encoding( encoding )
    , m_region( region )
    , m_bytes( bytes )
{
}

void VncFrame::setFrame( Encoding encoding,
    const QRect& region, const QByteArray& bytes )
{
    m_encoding = encoding;
    m_region = region;
    m_bytes = bytes;
}

const uint8_t* VncFrame::bytes() const
{
    return reinterpret_cast< const uint8_t* >( m_bytes.constData() );
}
