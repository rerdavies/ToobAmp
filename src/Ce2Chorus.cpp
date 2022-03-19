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

#include "Ce2Chorus.hpp"
#include "Filters/LowPassFilter.h"

using namespace TwoPlay;
using namespace LsNumerics;

constexpr float LFO_V0 = 4.5; // volts.
constexpr float LFO_MIN = 0.1;
constexpr float LFO_MAX = 6.5-LFO_V0;

constexpr float BUCKET_BRIGADE_V0_DELAY = 0.005; // s. total delay of the bucket brigade at Vlfo = LFO_V0.
constexpr float BUCKET_BRIGADE_V0_RATE = 1024/BUCKET_BRIGADE_V0_DELAY; // The bucket brigade clock rate at LFO_V0


Ce2Chorus::Ce2Chorus() { }
Ce2Chorus::Ce2Chorus(double sampleRate)
{
    SetSampleRate(sampleRate);
}

void Ce2Chorus::SetSampleRate(double sampleRate)
{
    const float MAX_DELAY_MS = 50;
    this->sampleRate = sampleRate;

    uint32_t maxDelay = sampleRate*MAX_DELAY_MS/1000;
    delayLine.SetMaxDelay(maxDelay);

    lfoLowpassFilter.SetSampleRate(sampleRate);
    lfoLowpassFilter.SetCutoffFrequency(76.0); 

    antiAliasingLowpassFilter.Design(sampleRate,0.5,5000,-25,20000);

    SetRate(this->rate);
    SetDepth(this->depth);
    Clear();

}

inline void Ce2Chorus::ClearBucketBrigade()
{
    auto averageDelay = BUCKET_BRIGADE_V0_DELAY*BUCKET_BRIGADE_SCALE;
    for (size_t i = 0; i < BUCKET_BRIGADE_LENGTH; ++i)
    {
        bucketBrigadeDelays[i] = averageDelay;
    }
    bucketBrigadeTotal = BUCKET_BRIGADE_V0_DELAY;
    bucketBrigadeIndex = 0;
    bbX = 0;

}
inline float Ce2Chorus::TickBucketBrigade(float voltage) {

    // guard against out-of-range voltage swings because of the lfo filter.
    if (voltage < 0.1f) voltage = 0.1f;
    if (voltage > 10) voltage = 10;

    // assume that clock frequency is linearly proportional to voltage.

    float fBB = BUCKET_BRIGADE_V0_RATE*voltage/LFO_V0;
    if (fBB < 1) fBB = 1; 

    float bbDelay = 1/fBB;

    double clocksThisSample = fBB/sampleRate + bbX;
    int iClocksThisSample = (int)clocksThisSample;
    bbX = clocksThisSample-iClocksThisSample;

    float t = bbDelay;
    for (int x = 0; x < iClocksThisSample; ++x)
    {
        float dx = (float)(t-bucketBrigadeDelays[bucketBrigadeIndex]);
        bucketBrigadeDelays[bucketBrigadeIndex] = t;
        bucketBrigadeTotal += dx;
        ++bucketBrigadeIndex;
        if (bucketBrigadeIndex >= BUCKET_BRIGADE_LENGTH)
        {
            bucketBrigadeIndex = 0;
        }
    }
    return bucketBrigadeTotal;

}

inline double Ce2Chorus::TickLfo()
{
    lfoValue += lfoDx;
    // one branch version of lfo update (-1 compare, -1 conditional brach,  +1 multiply)
    if (lfoValue >= 1)
    {
        lfoValue = lfoValue-2;
        lfoSign = -lfoSign;
    }
    float value =  lfoValue*lfoSign;
    // 76hz low pass (to match Ce2 analysis)
    value = lfoLowpassFilter.Tick(value);
    
    // The output of the LFO in VOLTS.
    value = value*depthFactor+LFO_V0;
    value = TickBucketBrigade(value);

    // calculate the total delay in a 1024-entry bucket brigade delay (to provide the correct shape)

    return value;
}


void Ce2Chorus::SetRate(float rate)
{
    this->rate = rate;

    float rateHz = 0.1*(1-rate) + 3.25*rate;
    this->lfoDx = 4*rateHz/(sampleRate); // *2 for 1/2 duty cycle cycle, *2 for [-1..1].

}
void Ce2Chorus::SetDepth(float depth)
{
    this->depth = depth;
    this->depthFactor = LFO_MIN*(1-depth)+LFO_MAX*depth;
}

float Ce2Chorus::Tick(float value)
{
    float delaySec = TickLfo();
    float delayValue = delayLine.Get(delaySec*sampleRate);
    delayLine.Put(value);
    return 0.5*(antiAliasingLowpassFilter.Tick(delayValue)+value);
}

void Ce2Chorus::Clear()
{
    delayLine.Clear();

    lfoValue = 0;
    lfoSign = 1;
    ClearBucketBrigade();
}

float Ce2Chorus::Instrumentation::TickLfo()
{
    float delay = pChorus->TickLfo();
    return delay;

}
