/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "RfbEncoder.h"

#include <qimage.h>
#include <qbuffer.h>
#include <qimagewriter.h>

class RfbEncoder::Encoder
{
  public:
    virtual ~Encoder() = default;

    virtual void encode( const QImage&, int compression ) = 0;
    virtual const QByteArray& encodedData() const = 0;
    virtual void release() = 0;
};

namespace
{
    class EncoderQt : public RfbEncoder::Encoder
    {
      public:
        EncoderQt()
        {
            m_imageWriter.setFormat( "jpeg" );
        }

        void encode( const QImage& image, int quality ) override
        {
            QBuffer buffer( &m_encodedData );

            m_imageWriter.setDevice( &buffer );
            m_imageWriter.setQuality( quality );
            m_imageWriter.write( image );
        }

        const QByteArray& encodedData() const override
        {
            return m_encodedData;
        }

        void release() override
        {
        }

      private:
        QImageWriter m_imageWriter;
        QByteArray m_encodedData;
    };
}

#ifdef VNC_VA_ENCODER

#include "va/VncVaEncoder.h"

namespace
{
    class EncoderVa : public RfbEncoder::Encoder
    {
      public:
        EncoderVa()
        {
            m_vaEncoder.open();
        }

        void encode( const QImage& image, int quality ) override
        {
            m_vaEncoder.encode( image.constBits(),
                image.width(), image.height(), quality );

            uint8_t* data;
            size_t size;

            m_vaEncoder.mapEncoded( data, size );
            m_encodedData.setRawData(
                reinterpret_cast< const char* >( data ), size );
        }

        const QByteArray& encodedData() const override
        {
            return m_encodedData;
        }

        void release() override
        {
            m_encodedData.clear();
            m_vaEncoder.unmapEncoded();
        }

      private:
        VncVaEncoder m_vaEncoder;
        QByteArray m_encodedData;
    };
}

#endif

RfbEncoder::RfbEncoder()
{
#ifdef VNC_VA_ENCODER
    m_encoder = new EncoderVa();
#else
    m_encoder = new EncoderQt();
#endif
}

RfbEncoder::~RfbEncoder()
{
    delete m_encoder;
}

#define DEBUG_ENCODING 0

#if DEBUG_ENCODING
    #include <qelapsedtimer.h>
#endif

void RfbEncoder::encode( const QImage& image, const QRect& rect )
{
#if DEBUG_ENCODING
    QElapsedTimer timer;
    timer.start();
#endif

    if ( rect == QRect( 0, 0, image.width(), image.height() ) )
        m_encoder->encode( image, m_quality );
    else
        m_encoder->encode( image.copy( rect ), m_quality );

#if DEBUG_ENCODING
    const auto ms = timer.elapsed();

    qDebug() << "JPEG:" << "quality:" << m_quality
        << "w:" << image.width() << "h:" << image.height()
        << "bytes:" << image.sizeInBytes()
        << "->" << m_encoder->encodedData().size()
        << "ms: elapsed" << ms;
#endif
}

const QByteArray& RfbEncoder::encodedData() const
{
    return m_encoder->encodedData();
}

void RfbEncoder::release()
{
    m_encoder->release();
}
