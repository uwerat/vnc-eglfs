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

    VncFrame() = default;

    VncFrame( Encoding, const QSize& );
    VncFrame( Encoding, int width, int height );

    VncFrame( Encoding, const QSize&, const uint8_t* );
    VncFrame( Encoding, int width, int height, const uint8_t* );

    VncFrame subFrame( const QRect& ) const;

    inline Encoding encoding() const { return m_encoding; }

    inline int width() const { return m_width; }
    inline int height() const { return m_height; }

    inline QSize size() const { return QSize( m_width, m_height ); }
    inline QRect boundingRect() const { return QRect( 0, 0, m_width, m_height ); }

    inline qsizetype byteCount() const { return m_bytes.size(); }

    const uint8_t* bytes() const;
    uint8_t* editableBytes();

    inline bool isValid() const { return !m_bytes.isEmpty(); }
    void reset();

    static VncFrame fromRawData( Encoding, int width, int height, const uint8_t* );
    static VncFrame fromByteArray( Encoding, int width, int height, const QByteArray& );

  private:
    VncFrame( Encoding, int width, int height, const QByteArray& );
    Encoding m_encoding = Raw;

    int m_width = 0;
    int m_height = 0;

    // using QByteArray because of QByteArray::fromRawData
    QByteArray m_bytes;
};

inline VncFrame::VncFrame( Encoding encoding, const QSize& size )
    : VncFrame( encoding, size.width(), size.height() )
{
}

inline VncFrame::VncFrame( Encoding encoding, const QSize& size, const uint8_t* bytes )
    : VncFrame( encoding, size.width(), size.height(), bytes )
{
}
