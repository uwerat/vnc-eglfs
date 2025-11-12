/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <vector>
#include <cinttypes>

namespace VncJpeg
{
    class Header
    {
      public:
        Header( int width, int height, int quality );

        const uint8_t* buffer() const { return m_buffer; }
        int count() const { return m_pos; }

      private:
        void addMarker( uint16_t );
        void add2x4( uint8_t, uint8_t );
        void add8( uint8_t );

        void add16( uint16_t val );
        void addBytes( const std::vector< uint8_t >& );
        void addBytes( const char*, size_t );

        void add( uint8_t val, int numBits );

        int m_pos = 0;
        uint8_t m_buffer[623] = {};
    };

    using Table = std::vector< uint8_t >;

    extern const Table dcCoefficientsLuminance;
    extern const Table dcCoefficientsChroma;
    extern const Table dcValues;

    extern const Table acCoefficientsLuminance;
    extern const Table acCoefficientsChroma;
    extern const Table acValuesLuminance;
    extern const Table acValuesChroma;

    extern const Table lumaQuantization;
    extern const Table chromaQuantization;
}
