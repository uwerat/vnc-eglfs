/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <cinttypes>
#include <va/va.h>

class VncVaRenderer;

class VncVaEncoder
{
  public:
    VncVaEncoder();
    ~VncVaEncoder();

    bool open();
    void close();

    bool isOpen() const;

    void encode( const uint8_t*, int width, int height, int quality );

    void mapEncoded( uint8_t*&, size_t& size );
    void unmapEncoded();

  private:
    bool openDisplay();
    void closeDisplay();

    void resizeSurface( int width, int height );
    void render( const uint8_t* );

    bool isEncoding() const;

  private:
    int m_fd = -1;
    VADisplay m_display = 0;

    VAConfigID m_configId = 0;
    VASurfaceID m_surfaceId = 0;
    VAContextID m_contextId = 0;

    VncVaRenderer* m_renderer = nullptr;
};
