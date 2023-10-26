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

namespace toob {
    class CombFilter {
    private:
        double sampleRate;
        double t;
        float combDepth;
        float combScale;

        IDelay delay; 
        IDelay delayR; 
    public:
        RangedInputPort Comb = RangedInputPort(0.0f,1.0f);
        RangedInputPort CombF = RangedInputPort(1000.0f,10000.0f);

        void SetSampleRate(double rate)
        {
            this->sampleRate = rate;
            this->t = 1.0/rate;
            int maxDelay = (int)ceil(rate/CombF.GetMinValue());
            delay.SetMaxDelay(maxDelay+1);
            delayR.SetMaxDelay(maxDelay+1);
        }
        void Reset()
        {
            delay.Reset();
            delayR.Reset();
        }
        bool UpdateControls()
        {
            bool changed = false;
            if (CombF.HasChanged())
            {
                float f = CombF.GetValue();
                uint32_t iDelay = (int32_t)(0.5*(sampleRate/f)+0.5f);
                delay.SetDelay(iDelay);
                delayR.SetDelay(iDelay);
                changed = true;
            }
            if (Comb.HasChanged())
            {
                combDepth = Comb.GetValue();
                combScale = (float)(1.0/(1+combDepth));
                changed = true;
            }
            return changed;
        }

        float Tick(float value)
        {
            float dly = delay.Tick(value);

            return (dly*combDepth+value)*combScale;

            return value;
        }
        float TickR(float value)
        {
            float dly = delayR.Tick(value);

            return (dly*combDepth+value)*combScale;

            return value;
        }

        float GetFrequencyResponse(float f)
        {
            double wf = f*(2*M_PI)*t;
            std::complex<double> zF = std::exp(std::complex<double>(0,wf));

            std::complex<double> zDelay = std::exp(std::complex<double>(0,wf*delay.GetDelay()));


            std::complex<double> result = (zF +zDelay*(double)combDepth)*(double)combScale;
            return  (float)std::abs(result);
        }

    };
}