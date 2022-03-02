/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#pragma once

#include "std.h"
#include <memory.h>
#include "LsNumerics/LsMath.hpp"


namespace TwoPlay {
    using namespace LsNumerics;

    class IDelay {
    private:
        float*buffer = NULL;
        int32_t ixMask = 0;
        int32_t head = 0;
        int32_t delay = 1;

    public:
        ~IDelay() {
            free(buffer);
            buffer = NULL;
        }
        void SetMaxDelay(int samples)
        {
            samples = NextPowerOfTwo(samples+1);
            if (samples-1 != ixMask)
            {
                if (buffer != NULL) free(buffer);
                buffer = NULL;
                buffer = (float*)calloc(samples,sizeof(float));
                ixMask = samples-1;
                Reset();
            }
        }
        void Reset()
        {
            if (buffer != NULL)
            {
                memset(buffer,0,ixMask+1);
            }
            head = 0;
        }
        void SetDelay(int32_t samples)
        {
            this->delay = samples;
        }
        int32_t GetDelay() const { return this->delay; }

        inline float Tick(float value)
        {
            int ix = this->head = (head-1) & ixMask;
            buffer[ix] = value;
            return buffer[(ix + delay) & ixMask];
        }
    };
}