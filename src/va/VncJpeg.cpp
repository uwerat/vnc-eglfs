#include "VncJpeg.h"
#include <cstring>
#include <arpa/inet.h>

namespace VncJpeg
{
    Table::Table( const std::initializer_list< uint8_t >& list )
        : std::vector< uint8_t >( list )
    {
    }

    void Table::copyTo( void* to ) const
    {
        memcpy( to, data(), size() );
    }
}

namespace VncJpeg
{
    QuantizationTable::QuantizationTable( const std::initializer_list< uint8_t >& list )
        : std::vector< uint8_t >( list )
    {
    }

    void QuantizationTable::copyTo( uint8_t* to ) const
    {
        for ( size_t i = 0; i < size(); i++ )
            to[i] = sequenced( i );
    }

    uint8_t QuantizationTable::sequenced( int i ) const
    {
        /*
            Zigzag scan order of the the Luma and Chroma components
            Note: Figure A.6 shows the zigzag order differently.
         */
        static const uint8_t zigzag[] =
        {
            0,   1,   8,   16,  9,   2,   3,   10,
            17,  24,  32,  25,  18,  11,  4,   5,
            12,  19,  26,  33,  40,  48,  41,  34,
            27,  20,  13,  6,   7,   14,  21,  28,
            35,  42,  49,  56,  57,  50,  43,  36,
            29,  22,  15,  23,  30,  37,  44,  51,
            58,  59,  52,  45,  38,  31,  39,  46,
            53,  60,  61,  54,  47,  55,  62,  63
        };

        return at( zigzag[i] );
    }
}

namespace
{
    inline uint32_t bounded255( uint32_t value )
    {
        if ( value > 255 )
            return 255;

        if ( value < 1 )
            return 1;

        return value;
    }

    // ISO/IEC 10918-1

    enum
    {
        SOI  = 0xFFD8, //Start of Image
        EOI  = 0xFFD9, //End of Image
        SOS  = 0xFFDA, //Start of Scan
        DQT  = 0xFFDB, //Define Quantization Table
        DRI  = 0xFFDD, //Define restart interval
        RST0 = 0xFFD0, //Restart interval termination
        DHT  = 0xFFC4, //Huffman table
        SOF0 = 0xFFC0, //Baseline DCT
        APP0 = 0xFFE0, //Application Segment
        COM  = 0xFFFE  //Comment segment
    };

    // Annex K, Table K.1
    const VncJpeg::QuantizationTable lumaQuant =
    {
        16, 11, 10, 16, 24,  40,  51,  61,
        12, 12, 14, 19, 26,  58,  60,  55,
        14, 13, 16, 24, 40,  57,  69,  56,
        14, 17, 22, 29, 51,  87,  80,  62,
        18, 22, 37, 56, 68,  109, 103, 77,
        24, 35, 55, 64, 81,  104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
    };

    //  Annex K, Table K.2
    const VncJpeg::QuantizationTable chromaQuant =
    {
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
    };

    // K.3.3.1 is the summarized version of Table K.3

    VncJpeg::Table dcCoefficientsLuminance =
    {
        0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    const VncJpeg::Table dcCoefficientsChroma =
    {
        0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    const VncJpeg::Table dcValues =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
    };

    //K.3.3.2 is the summarized version of Table K.5

    const VncJpeg::Table acCoefficientsLuminance =
    {
        0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
        0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D,
    };

    const VncJpeg::Table acValuesLuminance =
    {
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
        0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
        0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,

        0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
        0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
        0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
        0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
        0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
        0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,

        0xF9, 0xFA
    };

    // K.3.3.2 is the summarized version of Table K.6

    const VncJpeg::Table acCoefficientsChroma =
    {
        0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
        0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
    };

    const VncJpeg::Table acValuesChroma =
    {
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
        0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
        0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
        0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
        0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
        0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
        0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2,
        0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
        0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
        0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
        0xF9, 0xFA
    };
}

namespace VncJpeg
{
    const Table& dcCoefficientsLuminance() { return ::dcCoefficientsLuminance; }
    const Table& dcCoefficientsChroma() { return ::dcCoefficientsChroma; }
    const Table& dcValues() { return ::dcValues; }
    const Table& acCoefficientsLuminance() { return ::acCoefficientsLuminance; }
    const Table& acCoefficientsChroma() { return ::acCoefficientsChroma; }
    const Table& acValuesLuminance() { return ::acValuesLuminance; }
    const Table& acValuesChroma() { return ::acValuesChroma; }

    const QuantizationTable& lumaQuantization() { return ::lumaQuant; }
    const QuantizationTable& chromaQuantization() { return ::chromaQuant; }

    Header::Header( int width, int height, int quality )
    {
        add16( SOI ); // Start of Image

        // -- Application Segment

        add16( APP0 );  // Application Segment

        add16( 16 );           //Length excluding the marker
        addBytes( "JFIF", 5 ); // including '\0'
        add8( 1 );             //Major Version
        add8( 1 );             //Minor Version

    #if 0
        add8( 1 );   // 
        add16( 72 );
        add16( 72 );
    #else
        add8( 0 );  // 0: no units, 1: pixels per inch, 2: pixels per cm
        add16( 1 ); // X pixel-aspect-ratio
        add16( 1 ); // Y pixel-aspect-ratio
    #endif

        // Thumbnail width/height
        add8( 0 );
        add8( 0 );

        // -- Quantization Tables

        const int qFactor = ( quality < 50 ) ? ( 5000 / quality ) : ( 200 - ( quality * 2 ) );

        for ( int i = 0; i <= 1; i++ )
        {
            const auto& quant = ( i == 0 ) ? lumaQuant : chromaQuant;

            add16( DQT );
            add16( 3 + quant.size() );
            add2x4( 0, ( i == 0 ) ? 0 : 1 ); // Pq, Tq

            for ( size_t i = 0; i < quant.size(); i++ )
            {
                uint32_t qk = quant.sequenced( i );
                qk = bounded255( ( qk * qFactor ) / 100 );

                add8( qk );
            }
        }

        // -- Baseline DCT process frame

        add16( SOF0 );

        add16( 8 + 3 * 3 ); // num bytes

        add8( 8 );       // sample_precision
        add16( height );
        add16( width );

        add8( 3 );       // num_components

        add8( 1 );       // id
        add2x4( 1, 1 );  // horizontal/vertical factors
        add8( 0 );       // quant_table_selector

        add8( 2 );       // id
        add2x4( 1, 1 );  // horizontal/vertical factors
        add8( 1 );       // quant_table_selector

        add8( 3 );       // id
        add2x4( 1, 1 );  // horizontal/vertical factors
        add8( 1 );       // quant_table_selector

        // -- Huffman Tables

        add16( DHT );
        add16( 3 + dcCoefficientsLuminance().size() + dcValues().size() );
        add2x4( 0, 0 ); // DC, Huffman table id
        addBytes( dcCoefficientsLuminance() );
        addBytes( dcValues() );

        add16( DHT );
        add16( 3 + acCoefficientsLuminance().size() + acValuesLuminance().size() );
        add2x4( 1, 0 ); // AC, Huffman table id
        addBytes( acCoefficientsLuminance() );
        addBytes( acValuesLuminance() );

        add16( DHT );
        add16( 3 + dcCoefficientsChroma().size() + dcValues().size() );
        add2x4( 0, 1 ); // DC, Huffman table id
        addBytes( dcCoefficientsChroma() );
        addBytes( dcValues() );

        add16( DHT );
        add16( 3 + acCoefficientsChroma().size() + acValuesChroma().size() );
        add2x4( 1, 1 ); // AC, Huffman table id
        addBytes( acCoefficientsChroma() );
        addBytes( acValuesChroma() );

        // -- Start of Scan

        add16( SOS );
        add16( 3 + 3 * 2 + 3 );

        add8( 3 ); // num components

        add8( 1 ); // Y
        add2x4( 0, 0 );

        add8( 2 ); // U
        add2x4( 1, 1 );

        add8( 3 ); // V
        add2x4( 1, 1 );

        add8( 0 );
        add8( 63 ); // baseline
        add2x4( 0, 0 );
    }

    void Header::add2x4( uint8_t val1, uint8_t val2 )
    {
        add8( ( val2 & 0xf ) | ( val1 << 4 ) );
    }

    void Header::add8( uint8_t val )
    {
        m_buffer[m_pos++] = val;
    }

    void Header::add16( uint16_t val )
    {
        val = htons( val ); // ???

        add8( val & 0xff );
        add8( val >> 8 );
    }

    void Header::addBytes( const std::vector< uint8_t >& bytes )
    {
        memcpy( m_buffer + m_pos, bytes.data(), bytes.size() );
        m_pos += bytes.size();
    }

    void Header::addBytes( const char* bytes, size_t count )
    {
        memcpy( m_buffer + m_pos, bytes, count );
        m_pos += count;
    }
}
