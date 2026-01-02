/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qbytearray.h>
#include <qrect.h>

class VncFrame
{
  public:
    enum Encoding
    {
        Raw = 0,

        Rgb,
        Jpeg
    };

    VncFrame();
    VncFrame( Encoding, const QRect&, const QByteArray& );

    void setFrame( Encoding, const QRect&, const QByteArray& );

    Encoding encoding() const;

    int width() const;
    int height() const;
    QSize size() const;
    QRect region() const;

    size_t byteCount() const;
    const uint8_t* bytes() const;

    // only for RGB
    VncFrame encoded( int quality ) const;
    VncFrame clipped( const QRect& ) const;

  private:
    Encoding m_encoding = Raw;
    QRect m_region;
    QByteArray m_bytes;
};

inline VncFrame::Encoding VncFrame::encoding() const
{
    return m_encoding;
}

inline int VncFrame::width() const
{
    return m_region.width();
}

inline int VncFrame::height() const
{
    return m_region.height();
}

inline QSize VncFrame::size() const
{
    return m_region.size();
}

inline QRect VncFrame::region() const
{
    return m_region;
}

inline size_t VncFrame::byteCount() const
{
    return m_bytes.size();
}
