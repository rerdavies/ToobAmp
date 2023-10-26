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
#include "InputPort.h"
#include "IDelay.h"
#include <cmath>
#include <complex>
#include "restrict.hpp"
#include <numbers>


namespace toob {
    class CombFilter {
    public:
        static constexpr size_t FIR_LENGTH = 24;
    public:
        RangedInputPort Comb = RangedInputPort(0.0f,1.0f);
        RangedInputPort CombF = RangedInputPort(1000.0f,10000.0f);

        void SetSampleRate(double rate);

        void Reset()
        {
            memset(rightDelay,0,sizeof(rightDelay));
            memset(leftDelay,0,sizeof(leftDelay));
            delayIndex = 0;


        }
        bool UpdateControls()
        {
            bool changed = false;
            if (CombF.HasChanged() || Comb.HasChanged())
            {
                UpdateFilter(CombF.GetValue(),Comb.GetValue());
                changed = true;
            }
            return changed;
        }

        void UpdateFilter(float frequency, float depth);

        float Tick(float value)
        {
            float directSample = directSampleDelay.Tick(value);
            float combSample = combSampleDelay.Tick(value);
            if (delayIndex >= FIR_LENGTH)
            {
                delayIndex = 0;
            }
            leftDelay[delayIndex] = combSample;
            leftDelay[delayIndex+FIR_LENGTH] = combSample; // allows us to do a convolution without a branch in the loop.
            ++delayIndex;


            // restrict declaration allows conversion to SIMD without loop preamble on clang, GCC, and MSVC.
            float * restrict pFilter = firFilter;
            float * restrict pDelay = leftDelay+delayIndex;
            float sum = 0;
            for (size_t i = 0; i < FIR_LENGTH; ++i)
            {
                sum += pFilter[i]*pDelay[i];
            }

            return sum + directSample*directScale;
        }



        float GetFrequencyResponse(float f)
        {
            double wf = f*(2*std::numbers::pi)*t;
            std::complex<double> zF = std::exp(std::complex<double>(0,wf));

            std::complex<double> zDelay = std::exp(std::complex<double>(0,wf*(fDelay+1)));


            std::complex<double> result = zF*(double)(directScale) +zDelay*(double)delayScale;
            return  (float)std::abs(result);
        }


    private:
        IDelay directSampleDelay;
        IDelay combSampleDelay;

        double directScale = 0;
        double delayScale = 0;
        double sampleRate;
        double t;

        float leftDelay[FIR_LENGTH*2];
        float rightDelay[FIR_LENGTH*2];

        float firFilter[FIR_LENGTH];
        float window[FIR_LENGTH];
        size_t delayIndex;

        float fDelay = 0;
        

    };
}