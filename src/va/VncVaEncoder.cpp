#include "VncVaEncoder.h"
#include "VncVaRenderer.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <string>

#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>

#include <va/va.h>
#include <va/va_drm.h>

VncVaEncoder::VncVaEncoder()
{
}

VncVaEncoder::~VncVaEncoder()
{
    close();
}

bool VncVaEncoder::isOpen() const
{
    return m_display != 0;
}

bool VncVaEncoder::isEncoding() const
{
    VAEntrypoint entrypoints[5];
    int count;

    vaQueryConfigEntrypoints( m_display, VAProfileJPEGBaseline, entrypoints, &count );

    for ( int i = 0; i < count; i++ )
    {
        if ( entrypoints[i] == VAEntrypointEncPicture )
            return true;
    }

    return false;
}

bool VncVaEncoder::open()
{
    openDisplay();

    int major, minor;
    vaInitialize( m_display, &major, &minor );

    const bool ok = isEncoding();
    if ( !ok )
        close();

    return ok;
}

void VncVaEncoder::close()
{
    closeDisplay();
}

bool VncVaEncoder::openDisplay()
{
    for ( const auto name : { "renderD128", "card0" } )
    {
        const auto path = std::string( "/dev/dri/" ) + name;

        const auto fd = ::open( path.c_str(), O_RDWR);
        if ( fd >= 0 )
        {
            m_display = vaGetDisplayDRM( fd );

            if ( m_display )
            {
                fprintf( stderr,"Path: %s\n", path.c_str() ); // cerr
                m_fd = fd;

                return true;
            }

            ::close( fd );
        }
    }

    return false;
}

void VncVaEncoder::closeDisplay()
{
    if ( m_display != 0 )
    {
        vaTerminate( m_display );
        m_display = 0;
    }

    if ( m_fd >= 0 )
    {
        ::close( m_fd );
        m_fd = -1;
    }
}

void VncVaEncoder::resizeSurface( int width, int height )
{
    {
        VAConfigAttrib attrib[2];

        attrib[0].type = VAConfigAttribRTFormat;
        attrib[1].type = VAConfigAttribEncJPEG;

        vaGetConfigAttributes( m_display,
            VAProfileJPEGBaseline, VAEntrypointEncPicture, &attrib[0], 2 );

#if 1
        if ( !( attrib[0].value & VA_RT_FORMAT_RGB32 ) )
        {
            /* Did not find the supported RT format */
            assert(0);
        }
        attrib[0].value = VA_RT_FORMAT_RGB32;
#endif

        {
            VAConfigAttribValEncJPEG value; // union !
            value.value = attrib[1].value;

            auto& bits = value.bits;

            bits.arithmatic_coding_mode = 0;
            bits.progressive_dct_mode = 0;
            bits.non_interleaved_mode = 1;
            bits.differential_mode = 0;

#if 0
            bits.max_num_components = 3;
            bits.max_num_scans = 1;
            bits.max_num_huffman_tables = 2;
            bits.max_num_quantization_tables = 3;
#endif

            attrib[1].value = value.value;
        }

        vaCreateConfig( m_display, VAProfileJPEGBaseline, VAEntrypointEncPicture,
            attrib, 2, &m_configId );
    }

    {
        VASurfaceAttrib fourcc;
        fourcc.type = VASurfaceAttribPixelFormat;
        fourcc.flags = VA_SURFACE_ATTRIB_SETTABLE;
        fourcc.value.type = VAGenericValueTypeInteger;
        fourcc.value.value.i = VA_FOURCC_RGBA; // 4 bytes : r, g, b, unspecified

        vaCreateSurfaces( m_display, VA_RT_FORMAT_RGB32, width, height,
            &m_surfaceId, 1, &fourcc, 1 );
    }

    vaCreateContext( m_display, m_configId, width, height,
        VA_PROGRESSIVE, &m_surfaceId, 1, &m_contextId );
}

void VncVaEncoder::encode( const uint8_t* frame, int width, int height, int quality )
{
    if( quality >= 100 )
        quality = 100;

    if( quality <= 0 )
        quality = 1;

    // For the moment we recreate everything for each frame. TODO ...
    resizeSurface( width, height );

    VncVaRenderer renderer;
    renderer.initialize( m_display, m_configId, m_surfaceId, m_contextId );

    renderer.addBuffers( width, height, quality );
    renderer.render( frame );
    renderer.removeBuffers();

    vaDestroySurfaces( m_display, &m_surfaceId, 1 );
    vaDestroyContext( m_display, m_contextId );
    vaDestroyConfig( m_display, m_configId );

    m_surfaceId = m_contextId = m_configId = 0;
}
