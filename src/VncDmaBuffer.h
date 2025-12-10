#pragma once

#include <qglobal.h>
#include <qsize.h>

class QByteArray;

class VncDmaBuffer
{
  public:
    VncDmaBuffer();
    VncDmaBuffer( const QSize&, unsigned int );

    ~VncDmaBuffer();

    static bool isSupported();

    bool setTexture( const QSize&, unsigned int );

    int fd() const;
    int stride() const;
    int offset() const;
    uint64_t modifier() const;
    int fourcc() const;

    QSize size() const;

    void reset();
    bool isValid() const;

    QByteArray data() const;

  private:
    Q_DISABLE_COPY( VncDmaBuffer )

    void* m_image = nullptr;

    int m_fd = -1;
    int m_stride = 0;
    int m_offset = 0;

    uint64_t m_modifier = 0;
    int m_fourcc = 0;

    QSize m_size;
};
