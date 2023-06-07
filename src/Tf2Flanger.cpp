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

#include "Tf2Flanger.hpp"
#include "Filters/LowPassFilter.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace toob;
using namespace LsNumerics;




// Measurements taken from LTSPICE simulation.
constexpr float MAX_DELAY_MS = 60;

// LFO seconds per cycle.
constexpr float LFO_S_R0 = 14.77-3.55; // 11.2
constexpr float LFO_S_R05 = 10.68-4.97; // 5.71
constexpr float LFO_S_R075 = 4.27-1.38; // 2.89
constexpr float LFO_S_R08 = 3.54-1.17; // 2.37
constexpr float LFO_S_R1 = 1.104-1.021; // 0.83


// Measured value.
// constexpr float LFO_MIN_V_M0_D1 = 2.17f; // LFO min voltage measured before VR7, MANUAL = 1, DEPTH = 1
// constexpr float LFO_MAX_V_M0_D1 = 2.83f; // LFO min voltage measured before VR7, MANUAL = 1, DEPTH = 1

// expand the range by ~10%.
constexpr float LFO_MIN_V_M0_D1 = 2.0f; // LFO min voltage measured before VR7, MANUAL = 1, DEPTH = 1
constexpr float LFO_MAX_V_M0_D1 = 3.00f; // LFO min voltage measured before VR7, MANUAL = 1, DEPTH = 1



// Manual has no effect when Depth=1.
// constexpr float LFO_MIN_V_M1_D1 = 2.17f; // LFO min voltage measured before VR7, MANUAL = 0, DEPTH = 1
// constexpr float LFO_MAX_V_M1_D1 = 2.83f; // LFO min voltage measured before VR7, MANUAL = 0, DEPTH = 1


constexpr float MANUAL_V_M1_D0 = 0.814f; // MANUAL voltage measured before VR7, MANUAL=0, DEPTH=0
constexpr float MANUAL_V_M05_D0 = 2.31f; // MANUAL voltage measured before VR7, MANUAL=0.5, DEPTH=0
constexpr float MANUAL_V_M0_D0 = 3.7585f; // MANUAL voltage measured before VR7, MANUAL=1, DEPTH=0

// check values measured before R48
constexpr float LFO_BLEND_V_M1_D0 = 0.8142f; // Before R48 DEPTH=0, MANUAL=0.0
constexpr float LFO_BLEND_V_M05_D0 = 2.318f; // Before R48 DEPTH=0, MANUAL=0.5
constexpr float LFO_BLEND_V_M0_D0 = 3.756f; // Before R48 DEPTH=0, MANUAL=1

constexpr float LFO_BLEND_V_M1_D05 = 1.441f; // Before R48 DEPTH=0.5, MANUAL=0.0
constexpr float LFO_BLEND_V_M05_D05 = 1.753f; // Before R48 DEPTH=0.5, MANUAL=0.5
constexpr float LFO_BLEND_V_M0_D05_MIN = 2.837f; // Before R48 DEPTH=0.5, MANUAL=1
constexpr float LFO_BLEND_V_M0_D05_MAX = 3.145f; // Before R48 DEPTH=0.5, MANUAL=1

constexpr float LFO_BLEND_V_M1_D1_MIN = 2.1f; // Before R48 DEPTH=1, MANUAL=0
constexpr float LFO_BLEND_V_M1_D1_MAX = 2.82f; // Before R48 DEPTH=1, MANUAL=0
constexpr float LFO_BLEND_V_M0_D1_MIN = 2.17; // Before R48 DEPTH=1, MANUAL=1
constexpr float LFO_BLEND_V_M0_D1_MAX = 2.82f; // Before R48 DEPTH=1, MANUAL=1

// Values from service manual.
constexpr float DELAY_M0_CLOCK_FREQ = 1/0.000025f;  // 25us
constexpr float DELAY_M1_CLOCK_FREQ = 1/0.000002f;

constexpr float DELAY_M0_CLOCK_T = 1/DELAY_M0_CLOCK_FREQ;  // 25us
constexpr float DELAY_M1_CLOCK_T = 1/DELAY_M1_CLOCK_FREQ;


Tf2Flanger::Tf2Flanger() { }
Tf2Flanger::Tf2Flanger(double sampleRate)
{
    SetSampleRate(sampleRate);
}

void Tf2Flanger::SetSampleRate(double sampleRate)
{
    const float MAX_DELAY_MS = 50;
    this->sampleRate = sampleRate;

    this->m0SamplesPerSec = sampleRate*1024/DELAY_M0_CLOCK_FREQ;
    this->m1SamplesPerSec = sampleRate*1024/DELAY_M1_CLOCK_FREQ;


    uint32_t maxDelay = sampleRate*MAX_DELAY_MS/1000;
    delayLine.SetMaxDelay(maxDelay);

    lfoLowpassFilter.SetSampleRate(sampleRate);
    lfoLowpassFilter.SetCutoffFrequency(45.00f); 


    constexpr float delayCutoff = 20000;
    preDelayLowPass1.SetSampleRate(sampleRate);
    preDelayLowPass2.SetSampleRate(sampleRate);
    postDelayLowPass.SetSampleRate(sampleRate);
    preDelayHighPass.SetSampleRate(sampleRate);
    
    preDelayLowPass1.SetCutoffFrequency(delayCutoff);
    preDelayLowPass2.SetCutoffFrequency(delayCutoff);
    postDelayLowPass.SetCutoffFrequency(delayCutoff);


    preDelayHighPass.SetCutoffFrequency(70);

    preemphasisFilter.SetSampleRate(sampleRate);
    deemphasisFilterL.SetSampleRate(sampleRate);
    deemphasisFilterR.SetSampleRate(sampleRate);
    preemphasisFilter.Design(0,15,1000);
    deemphasisFilterL.Design(0,-15,1000);
    deemphasisFilterR.Design(0,-15,1000);






    antiAliasingLowpassFilter.Design(sampleRate,0.5,20000,-25,22050);



    SetManual(this->manual);
    SetRate(this->rate);
    SetDepth(this->depth);
    SetRes(this->res);

    Clear();

}
inline double Tf2Flanger::LfoToVoltage(double lfoValue) {
        
    // The output of the LFO in VOLTS.
    float manualV = MANUAL_V_M0_D0 + (1-this->manual)*(MANUAL_V_M1_D0-MANUAL_V_M0_D0);
    float lfoV = (lfoValue*0.5+0.5)*(LFO_MAX_V_M0_D1-LFO_MIN_V_M0_D1) + LFO_MIN_V_M0_D1;
    float v = this->depth*lfoV + (1-this->depth)*manualV;
    return v;


}

inline double Tf2Flanger::LfoToFreq(double lfoValue)
{
    float voltage = LfoToVoltage(lfoValue);
    // assume that clock t  is linearly proportional to voltage.
    double vStd = (voltage-MANUAL_V_M1_D0)/(MANUAL_V_M0_D0-MANUAL_V_M1_D0);

    double t =  DELAY_M1_CLOCK_T + vStd*(DELAY_M0_CLOCK_T-DELAY_M1_CLOCK_T);
    return 1.0/t;
}

inline void Tf2Flanger::ClearBucketBrigade()
{
    lfoValue = 0;
    lfoSign = 1;

    double fBB = LfoToFreq(0);
    double bucketDelay = 1/fBB;
    lfoLowpassFilter.Reset();
    for (size_t i = 0; i < BUCKET_BRIGADE_LENGTH; ++i)
    {
        bucketBrigadeDelays[i] = bucketDelay;
        lfoLowpassFilter.Tick(0);
    }
    bucketBrigadeIndex = 0;
    bucketBrigadeTotal = bucketDelay*1024;
    bbX = 0;

}


inline float Tf2Flanger::TickBucketBrigade(float lfoValue) {

    double fBB = LfoToFreq(lfoValue);


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


inline double Tf2Flanger::TickLfo()
{
    lfoValue += lfoDx;
    // one branch version of lfo update (-1 compare, -1 conditional brach,  +1 multiply)
    if (lfoValue >= 1)
    {
        lfoValue = lfoValue-2;
        lfoSign = -lfoSign;
    }
    float value =  lfoValue*lfoSign;
    // lfo low pass (to match Bf2 analysis)
    value = lfoLowpassFilter.Tick(value);
    value = TickBucketBrigade(value);

    return value;
}


void Tf2Flanger::SetManual(float value)
{
    this->manual = value;
    UpdateLfoRange();
}
void Tf2Flanger::SetDepth(float depth)
{
    this->depth = depth;
    UpdateLfoRange();
}

void Tf2Flanger::SetRes(float value)
{
    this->res = value;
}

static inline float VBlend(float value, float v0, float v1)
{
    return (1-value)*v0+(value)*v1;
}
inline void Tf2Flanger::UpdateLfoRange()
{
    using namespace std;

    // double minF = LfoToFreq(-1);
    // double midF = LfoToFreq(0);
    // double maxF = LfoToFreq(1);



}

void Tf2Flanger::SetRate(float rate)
{
    this->rate = rate;

    double seconds = rate*LFO_S_R1 + (1.0f-rate)*LFO_S_R0;
    this->lfoDx = 4/(sampleRate*seconds); // *2 for half duty cycle, *2 for [-1..1].
    

}

float Tf2Flanger::Tick(float value)
{
    assert(value < 10.0);
    value = preemphasisFilter.Tick(value);
    float delaySec = TickLfo();
    float delayValue = delayLine.Get(delaySec*sampleRate);

    delayValue = antiAliasingLowpassFilter.Tick(delayValue);
    // TODO: delay is hard-clipped. Should really be diode soft-clipped.
    if (delayValue > 1.0) 
        delayValue = 1.0;
    if (delayValue < -1.0) 
        delayValue = -1.0; 

    double delayInput = value + this->res*delayValue;
    // delayInput = this->preDelayLowPass1.Tick(delayInput);
    // delayInput = this->preDelayLowPass2.Tick(delayInput);
    delayInput = this->preDelayHighPass.Tick(delayInput);
    float t = preDelayLowPass1.GetFrequencyResponse(22000);
    (void)t;

    delayLine.Put(delayInput);

    assert(delayValue < 10.0);

    float result =  deemphasisFilterL.Tick(delayValue+value);
    assert(result < 10.0);
    return result;
    //return delayValue;
}
void Tf2Flanger::Tick(float value, float*outL, float*outR)
{
    assert(value < 10.0);
    value = preemphasisFilter.Tick(value);
    float delaySec = TickLfo();
    float delayValue = delayLine.Get(delaySec*sampleRate);

    delayValue = antiAliasingLowpassFilter.Tick(delayValue);
    // TODO: delay is hard-clipped. Should really be diode soft-clipped.
    if (delayValue > 1.0) 
        delayValue = 1.0;
    if (delayValue < -1.0) 
        delayValue = -1.0; 

    double delayInput = value + this->res*delayValue;
    // delayInput = this->preDelayLowPass1.Tick(delayInput);
    // delayInput = this->preDelayLowPass2.Tick(delayInput);
    delayInput = this->preDelayHighPass.Tick(delayInput);
    delayLine.Put(delayInput);

    assert(delayValue < 10.0);    
    //return delayValue;
    *outL = deemphasisFilterL.Tick(value+delayValue);
    *outR = deemphasisFilterR.Tick(value-delayValue);

    assert(*outL < 10.0);    
    assert(*outR < 10.0);    

}

void Tf2Flanger::Clear()
{
    delayLine.Clear();

    lfoValue = 0;
    lfoSign = 1;
    ClearBucketBrigade();
}

float Tf2Flanger::Instrumentation::TickLfo()
{
    float delay = pFlanger->TickLfo();
    return delay;

}
