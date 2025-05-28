/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

class QImage;
class QRect;
class QByteArray;

class RfbEncoder
{
  public:
    RfbEncoder();
    ~RfbEncoder();

    void setQuality( int compression );
    int quality() const;

    void encode( const QImage&, const QRect& );
    const QByteArray& encodedData() const;

    void release();

    class Encoder;

  private:
    Encoder* m_encoder;

    int m_quality = 50;
};

inline void RfbEncoder::setQuality( int quality )
{
    m_quality = quality;
}

inline int RfbEncoder::quality() const
{
    return m_quality;
}
