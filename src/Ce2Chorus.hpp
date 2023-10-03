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

#include "LsNumerics/InterpolatingDelay.hpp"
#include "Filters/LowPassFilter.h"
#include "Filters/ChebyshevDownsamplingFilter.h"

namespace toob
{
    using namespace LsNumerics;

    // Emulation of a famous Chorus pedal.
    class Ce2Chorus {
    public:
        Ce2Chorus();
        Ce2Chorus(double sampleRate);

        void SetSampleRate(double sampleRate);

        void SetRate(float value); // [0..1]
        void SetDepth(float depth); // [..1]

        float Tick(float value);
        void Tick(float value,float*outL, float*outR);

        void Clear();

        class Instrumentation // test instrumentation
        {
        private:
            Ce2Chorus *pChorus;
        public:
            Instrumentation(Ce2Chorus *pChorus)
            : pChorus(pChorus)
            {

            }
            float TickLfo();
             
        };
    private:
        uint32_t bucketBrigadeIndex;
        static constexpr size_t BUCKET_BRIGADE_LENGTH = 1024;
        static constexpr double BUCKET_BRIGADE_SCALE = 1.0/BUCKET_BRIGADE_LENGTH;
        float bucketBrigadeDelays[BUCKET_BRIGADE_LENGTH];
        float bucketBrigadeTotal;
        double bbX = 0;

        void ClearBucketBrigade();
        float TickBucketBrigade(float value);


        double TickLfo();
        double sampleRate = 44100;
        float rate = 0.5f;
        float depth = 0.5f;
        float depthFactor = 0;

        float lfoValue = 0;


        float lfoDx = 0;
        float lfoSign = 1;

        InterpolatingDelay delayLine;
        LowPassFilter lfoLowpassFilter;
        ChebyshevDownsamplingFilter antiAliasingLowpassFilter;
    };
}