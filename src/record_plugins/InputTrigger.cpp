// Copyright (c) 2025 Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "InputTrigger.hpp"
#include <cmath>

using namespace toob;

InputTrigger::InputTrigger() {
    ThresholdDb(-25.0f);
}
void InputTrigger::Init(double sampleRate)
{
    triggerSamples = (size_t)(sampleRate / 10);    
    triggerLedCount = 0;
}

void InputTrigger::ThresholdDb(float value)
{
    if (value != this->thresholdDb) {
        this->thresholdDb = value;
        thresholdAfSquared = pow(10.0f, thresholdDb * (2.0/ 20.0f));
    }
}   

void InputTrigger::Run(const float *inL, const float *inR, size_t n_samples)
{
    triggered = false;
    if (inR != nullptr) {
        for (size_t i = 0; i < n_samples; ++i)
        {
            float af = inL[i] * inL[i] + inR[i] * inR[i];
            if (af > thresholdAfSquared)
            {
                triggerFrame = i;
                triggered = true;
                triggerLedCount = triggerSamples;
                break;
            }
        }
    } else {
        for (size_t i = 0; i < n_samples; ++i)
        {
            float af = inL[i] * inL[i];
            if (af > thresholdAfSquared)
            {
                triggerFrame = i;
                triggered = true;
                triggerLedCount = triggerSamples;
                break;
            }
        }
    }
    if (triggerLedCount > n_samples    )
    {
        triggerLedCount -= n_samples;
    }
    else
    {
        triggerLedCount = 0;
    }
}
