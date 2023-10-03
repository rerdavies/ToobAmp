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

#include "CombFilter2.h"
#include <numbers>

using namespace toob;

void CombFilter::SetSampleRate(double rate)
{
    this->sampleRate = rate;
    this->t = 1.0/rate;

    double dx = std::numbers::pi * 2/FIR_LENGTH;

    for (size_t i = 0; i < FIR_LENGTH; ++i)
    {
        double x = dx*i;
        double y = (float)(0.5*(1-std::cos(x)));
        window[i] = y;
    }
    int maxDelay =  (int)std::ceil(sampleRate/CombF.GetMinValue());
    directSampleDelay.SetMaxDelay(maxDelay);
    combSampleDelay.SetMaxDelay(512);
}

void CombFilter::UpdateFilter(float frequency, float depth)
{
    // frequency is the frequency of the first notch, so  frequency=f0/2
    float fDelay = sampleRate/(frequency*2);    
    this->fDelay = fDelay;
    int iDelay = (int)std::round(fDelay);
    float delayFraction = fDelay-iDelay;

    if (iDelay >= FIR_LENGTH/2-1)
    {
        directSampleDelay.SetDelay(0);
        combSampleDelay.SetDelay(iDelay-FIR_LENGTH/2+1);
        iDelay = FIR_LENGTH/2;
    } else {
        combSampleDelay.SetDelay(0);
        directSampleDelay.SetDelay(FIR_LENGTH/2-iDelay-1);
    }

    float delayScale = depth*0.5f;
    this->delayScale = delayScale;

    float directScale = 1.0-delayScale;
    this->directScale = directScale;

    //delayFraction = 0; // xxx delete me
    if (delayFraction == 0)
    {
        for (size_t i = 0; i < FIR_LENGTH; ++i)
        {
            firFilter[i] = 0;
        }
        firFilter[FIR_LENGTH/2] = delayScale;
    } else {
        double sum = 0;
        for (int i = 0; i < (int)FIR_LENGTH; ++i)
        {
            float x = std::numbers::pi*(i-int(FIR_LENGTH/2)+delayFraction);
            float y = std::sin(x)/x * window[i];
            sum += y;
            firFilter[i] = (y * delayScale);
        }
        double norm = 1/sum;
        for (int i = 0; i < (int)FIR_LENGTH; ++i)
        {
            firFilter[i] *= norm;
        }
    }
}


