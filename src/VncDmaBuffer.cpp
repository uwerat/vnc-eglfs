#include "VncDmaBuffer.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <fcntl.h>
#include <unistd.h>
#include <cstdint>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <drm/drm_fourcc.h>

#include <qsize.h>
#include <qdebug.h>

namespace
{
    #define CASE_STR( value ) case value: return #value;

    const char* getEglErrorString()
    {
        switch( eglGetError() )
        {
            CASE_STR( EGL_SUCCESS             )
            CASE_STR( EGL_NOT_INITIALIZED     )
            CASE_STR( EGL_BAD_ACCESS          )
            CASE_STR( EGL_BAD_ALLOC           )
            CASE_STR( EGL_BAD_ATTRIBUTE       )
            CASE_STR( EGL_BAD_CONTEXT         )
            CASE_STR( EGL_BAD_CONFIG          )
            CASE_STR( EGL_BAD_CURRENT_SURFACE )
            CASE_STR( EGL_BAD_DISPLAY         )
            CASE_STR( EGL_BAD_SURFACE         )
            CASE_STR( EGL_BAD_MATCH           )
            CASE_STR( EGL_BAD_PARAMETER       )
            CASE_STR( EGL_BAD_NATIVE_PIXMAP   )
            CASE_STR( EGL_BAD_NATIVE_WINDOW   )
            CASE_STR( EGL_CONTEXT_LOST        )
            default: return "Unknown";
        }
    }

    #undef CASE_STR

    auto eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");

    auto eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");

    auto eglExportDMABUFImageMESA =
        (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");

    auto eglExportDMABUFImageQueryMESA =
        (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC) eglGetProcAddress("eglExportDMABUFImageQueryMESA");

}

bool VncDmaBuffer::isSupported()
{
    if ( eglCreateImageKHR && eglDestroyImageKHR
         && eglExportDMABUFImageMESA && eglExportDMABUFImageQueryMESA )
    {
        return eglGetCurrentDisplay() && eglGetCurrentContext();
    }

    return false;
}

VncDmaBuffer::VncDmaBuffer()
{
}

VncDmaBuffer::VncDmaBuffer( const QSize& size, unsigned int textureId )
{
    setTexture( size, textureId );
}

VncDmaBuffer::~VncDmaBuffer()
{
    reset();
}

void VncDmaBuffer::reset()
{
    if ( m_image != EGL_NO_IMAGE_KHR )
    {
        eglDestroyImageKHR( eglGetCurrentDisplay(), m_image );
        m_image = EGL_NO_IMAGE_KHR;
    }

    if ( m_fd >= 0 )
    {
        ::close( m_fd );
        m_fd = -1;
    }

    m_size = QSize();
    m_offset = m_stride = m_modifier = 0;
}

bool VncDmaBuffer::isValid() const
{
    return ( m_image != EGL_NO_IMAGE_KHR ) && ( m_fd >= 0 );
}

bool VncDmaBuffer::setTexture( const QSize& size, unsigned int textureId )
{
    if ( textureId == 0 )
    {
        reset();
        return false;
    }

    Q_ASSERT( eglCreateImageKHR && eglExportDMABUFImageMESA );

    auto eglDisplay = eglGetCurrentDisplay();
    auto eglContext = eglGetCurrentContext();

    Q_ASSERT( eglDisplay && eglContext != EGL_NO_CONTEXT );

    //EGLint attrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
    EGLint attrs[] = { EGL_NONE };

    m_image = eglCreateImageKHR( eglDisplay, eglContext,
        EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)(uintptr_t)textureId, attrs);

    if ( m_image == EGL_NO_IMAGE_KHR )
    {
        qWarning() << "eglCreateImageKHR failed:" << textureId << getEglErrorString();
        return false;
    }

    int fds[4] = { -1, -1, -1, -1 };
    int strides[4], offsets[4];

    EGLBoolean ok = eglExportDMABUFImageMESA(
        eglDisplay, m_image, fds, strides, offsets );

    if (!ok)
    {
        qWarning() << "eglExportDMABUFImageMESA failed:" << getEglErrorString();
        return false;
    }

    Q_ASSERT( fds[1] == -1 );

    m_fd = fds[0];
    m_stride = strides[0];
    m_offset = offsets[0];

    int fourcc[4] = {};
    int num_planes[4] = {};
    EGLuint64KHR modifiers[4] = {};

    ok = eglExportDMABUFImageQueryMESA( eglDisplay, m_image,
        fourcc, num_planes, modifiers );
    if ( ok )
    {
        m_modifier = modifiers[0];
        m_fourcc = fourcc[0];
    }

    m_size = size;

    return true;
}

QByteArray VncDmaBuffer::data() const
{
    if ( !isValid() )
        return QByteArray();

    const int width = m_size.width();
    const int height = m_size.height();

    const auto mapSize = m_stride * height;

    void* mapped = mmap( nullptr, mapSize,
        PROT_READ, MAP_SHARED, m_fd, m_offset );

    if ( mapped == MAP_FAILED )
        return {};

    auto* src = static_cast< uint8_t* >( mapped );

    QByteArray bytes;
    bytes.resize( width * height );

    uint8_t* dst = reinterpret_cast< uint8_t* >( bytes.data() );

    for ( int row = 0; row < height; row++ )
    {
        memcpy( dst, src, width );

        src += m_stride;
        dst += width;
    }

    munmap(mapped, mapSize );
    return bytes;
}

QSize VncDmaBuffer::size() const
{
    return m_size;
}

int VncDmaBuffer::fd() const
{
    return m_fd;
}

int VncDmaBuffer::stride() const
{
    return m_stride;
}

int VncDmaBuffer::offset() const
{
    return m_offset;
}

uint64_t VncDmaBuffer::modifier() const
{
    return m_modifier;
}

int VncDmaBuffer::fourcc() const
{
    return m_fourcc;
}
