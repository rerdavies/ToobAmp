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
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include "LsMath.hpp"

namespace LsNumerics
{

    class InterpolatingDelay {
    public:
        InterpolatingDelay();
        InterpolatingDelay(uint32_t maxDelay);
        void SetMaxDelay(uint32_t maxDelay);
        void Clear();

        void Put(float value)
        {
            delayIndex = (delayIndex-1) & indexMask;
            delayLine[delayIndex] = value;
        }
        float Get(uint32_t index) const {
            return delayLine[(delayIndex + index) & indexMask];
        }
        float Get(double index) const {
            uint32_t iIndex = (uint32_t)index;
            double frac = index-iIndex;
            float v0 = Get(iIndex);
            float v1 = Get(iIndex+1);
            return (v0*(1-frac)+v1*frac);
        }
        float Get(float index) const {
            uint32_t iIndex = (uint32_t)index;
            double frac = index-iIndex;
            float v0 = Get(iIndex);
            float v1 = Get(iIndex+1);
            return (float)(v0*(1-frac)+v1*frac);
        }

    private:
        uint32_t delayIndex = 0;
        uint32_t indexMask = 0;
        std::vector<float> delayLine;

    };

} // namespace