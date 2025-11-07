/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#include "RfbEncoder.h"

#include <qimage.h>
#include <qbuffer.h>
#include <qimagewriter.h>
#include <qelapsedtimer.h>
#include <qloggingcategory.h>

Q_LOGGING_CATEGORY( logEncoding, "vnceglfs.encode", QtCriticalMsg )

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
            m_encodedData.resize( 0 );
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


void RfbEncoder::encode( const QImage& image, const QRect& rect )
{
    QElapsedTimer timer;

    if ( logEncoding().isDebugEnabled() )
        timer.start();

    if ( rect == QRect( 0, 0, image.width(), image.height() ) )
        m_encoder->encode( image, m_quality );
    else
        m_encoder->encode( image.copy( rect ), m_quality );

    if ( logEncoding().isDebugEnabled() )
    {
        const auto ms = timer.elapsed();

        qCDebug( logEncoding ) << "JPEG:" << "quality:" << m_quality
            << "w:" << image.width() << "h:" << image.height()
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
            << "bytes:" << image.sizeInBytes()
#else
            << "bytes:" << image.byteCount()
#endif
            << "->" << m_encoder->encodedData().size()
            << "ms: elapsed" << ms;
    }
}

const QByteArray& RfbEncoder::encodedData() const
{
    return m_encoder->encodedData();
}

void RfbEncoder::release()
{
    m_encoder->release();
}
