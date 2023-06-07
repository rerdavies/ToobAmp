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
#include "Filters/HighPassFilter.h"
#include "Filters/ChebyshevDownsamplingFilter.h"
#include "Filters/ShelvingLowCutFilter2.h"

namespace toob
{
    using namespace LsNumerics;

    /// Emulation of a famous classic flanger.
    /// Functional emulation based on circuit analysis.
    class Tf2Flanger {
    public:
        Tf2Flanger();
        Tf2Flanger(double sampleRate);

        void SetSampleRate(double sampleRate);

        void SetManual(float value); // [0..1]
        void SetRate(float value); // [0..1]
        void SetDepth(float depth); // [0..1]
        void SetRes(float value); // [0..1]

        float Tick(float value);
        void Tick(float value, float*outL, float*outR);
        void Clear();

        float GetLfoValue() const { return this->lfoValue*this->lfoSign; }
        // test instrumentation
        class Instrumentation 
        {
        private:
            Tf2Flanger *pFlanger;
        public:
            Instrumentation(Tf2Flanger *pFlanger)
            : pFlanger(pFlanger)
            {

            }
            float TickLfo();
             
        };
    private:
        uint32_t bucketBrigadeIndex;
        double bucketBrigadeTotal = 0;
        static constexpr size_t BUCKET_BRIGADE_LENGTH = 1024;
        static constexpr double BUCKET_BRIGADE_SCALE = 1.0/BUCKET_BRIGADE_LENGTH;
        float bucketBrigadeDelays[BUCKET_BRIGADE_LENGTH];
        double bbX = 0;

        double LfoToVoltage(double lfoValue);
        double LfoToFreq(double lfoVoltage);
        void UpdateLfoRange();
        void ClearBucketBrigade();
        float TickBucketBrigade(float value);


        double TickLfo();
        double sampleRate = 44100;
        float manual = 0.5f;
        float rate = 0.5f;
        float depth = 0.5f;
        float res = 0.5f;

        double m0SamplesPerSec;
        double m1SamplesPerSec;


        float lfoValue = 0;
        float lfoDx = 0;
        float lfoSign = 1;

        InterpolatingDelay delayLine;
        LowPassFilter lfoLowpassFilter;



        LowPassFilter preDelayLowPass1;
        LowPassFilter preDelayLowPass2;
        HighPassFilter preDelayHighPass;
        LowPassFilter postDelayLowPass;

        ShelvingLowCutFilter2 preemphasisFilter;
        ShelvingLowCutFilter2 deemphasisFilterL;
        ShelvingLowCutFilter2 deemphasisFilterR;

        ChebyshevDownsamplingFilter antiAliasingLowpassFilter;

    };
}