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
#include "InputPort.h"
#include <cmath>
#include "LsNumerics/LsMath.hpp"

namespace TwoPlay{
    using namespace LsNumerics;

    class SagProcessor {
    private:
        LowPassFilter powerFilter;
        float currentPower = 0.0f;
        float currentSag = 1;
        float currentSagD = 1;
        float sagAf = 1;
        float sagDAf = 1;
    public:
        RangedInputPort Sag;
        RangedInputPort SagD;
        RangedInputPort SagF;
        
    public:
        SagProcessor()
        :Sag(0,1),
         SagD(0,1),
         SagF(5.0f,25.0f)
         {

         }
        void SetSampleRate(double rate) 
        {
            powerFilter.SetSampleRate(rate);
            powerFilter.SetCutoffFrequency(13.0);
        }

        void Reset()
        {
            powerFilter.Reset();
            currentSagD = 1.0f;
            currentSag = 1.0f;
        }

        void UpdateControls()
        {
            if (Sag.HasChanged())
            {
                float val = Sag.GetValue();
                float dbSag = val * 30;
                sagAf = Db2Af(dbSag);
            }
            if (SagD.HasChanged())
            {
                float val = SagD.GetValue();
                float dbSagD = val*30;
                sagDAf = Db2Af(dbSagD); 
            }
            if (SagF.HasChanged())
            {
                float val = SagF.GetValue();
                powerFilter.SetCutoffFrequency(val);
            }
        }
        inline float GetSagDValue() 
        {
            return currentSagD;
        }
        inline float GetSagValue() 
        {
            return currentSag;
        }
        inline float TickOutput(float value)
        {
            float powerInput = value*currentSagD;

            currentPower = std::fabs(powerFilter.Tick(powerInput*powerInput));
            currentSag = 1.0f/(currentPower*(sagAf-1)+1);
            currentSagD = 1.0f/(currentPower*(sagDAf-1)+1);

            return value;


        }

    }
;}