/*
 *   Copyright (c) 2021 Robin E. R. Davies
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

#include "Filters/LowPassFilter.h"
#include "Filters/DownsamplingLowPassFilter.h"

#define OLD_GAIN_FILTER 0

namespace TwoPlay {
    class GainStage {
    public:
        enum class EShape {
            ATAN = 0,
            TUBE_CLASSA,
            TUBE_CLASSB
        };
    private:
        LowPassFilter upsamplingFilter;
        #if OLD_GAIN_FILTER
            LowPassFilter downsamplingFilter;
        #else
            DownsamplingLowPassFilter downsamplingFilter;
        #endif
        double lastGainIn;

        double tubeMIn,tubeCIn,tubeMOut,tubeCOut;

        double gain;
        double gainScale;

        double GainFn(double value);

        EShape shape = EShape::ATAN;

        void SetClassAGain(float value);
        void SetClassBGain(float value);
    public:
        void SetShape(EShape shape);

        void Reset() 
        {
            upsamplingFilter.Reset();
            downsamplingFilter.Reset();
        }
        void SetGain(float value);

        void SetSampleRate(double rate);
        float Tick(float value);
    };
}