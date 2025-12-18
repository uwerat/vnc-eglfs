/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <cstdint>

namespace VncEgl
{
    class DmaBuffer
    {
      public:
        int fd = -1;
        int stride = 0;
        int offset = 0;

        uint64_t modifier = 0;
        int fourcc = 0;
    };

    DmaBuffer dmaBuffer( unsigned int texture );
    const char* devicePath();

    bool isSufficient();
}
