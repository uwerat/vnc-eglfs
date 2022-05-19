/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
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
