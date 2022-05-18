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

    void setCompression( int compression );
    int compression() const;

    void encode( const QImage&, const QRect& );
    const QByteArray& encodedData() const;

    void release();

    class Encoder;

  private:
    Encoder* m_encoder;

    int m_compression = 50;
};

inline void RfbEncoder::setCompression( int compression )
{
    m_compression = compression;
}

inline int RfbEncoder::compression() const
{
    return m_compression;
}

;
