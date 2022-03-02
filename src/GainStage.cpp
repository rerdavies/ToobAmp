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

#include "std.h"

#include "GainStage.h"
#include <cmath>
#include "db.h"



using namespace TwoPlay;


static const float CUTOFF_FREQUENCY = 16000;

void GainStage::SetSampleRate(double rate)
{
    this->upsamplingFilter.SetSampleRate(rate*4);
    this->downsamplingFilter.SetSampleRate(rate*4);
    this->upsamplingFilter.SetCutoffFrequency(CUTOFF_FREQUENCY);
    #if OLD_GAIN_FILTER
        this->downsamplingFilter.SetCutoffFrequency(CUTOFF_FREQUENCY);
    #endif
}



static inline double TubeFn(double value)
{
    return -LsNumerics::gTubeStageApproximation.At(value);
}


double GainStage::GainFn(double value)
{
    if (shape == EShape::ATAN)
        return (Atan(value*effectiveGain-bias)+postAdd)*gainScale;
    else 
        return (TubeFn(value*effectiveGain-bias)+postAdd)*gainScale;
}

void GainStage::SetShape(GainStage::EShape shape)
{
    this->shape = shape;
    UpdateShape();
}
void GainStage::SetBias(float bias)
{
    this->bias = bias;
    UpdateShape();
}
void GainStage::SetGain(float value)
{
    gain  = value;
    UpdateShape();
}

static inline float Blend(float value,float min, float max)
{
    return min + value*(max-min);
}
void GainStage::UpdateShape()
{
    switch (this->shape)
    {
        case EShape::ATAN:
        {
            double value = db2a(Blend(gain,-20,50));
            // (Atan((value*gain-bias)+postAdd)*gainScale;
            if (value < 1E-7f)
            {
                value = 1E-7F;
            }
            this->effectiveGain = value;

            double yZero = Atan(-bias);
            double yMax = Atan(1*effectiveGain-bias);
            double yMin = Atan(-1*effectiveGain-bias);

            postAdd = -yZero; 
            double max = std::max( yMax+postAdd, -(yMin+postAdd));

            this->gainScale = 1.0/max;
        }
        break;
        case EShape::TUBE:
            SetTubeGain(gain);
            break;
    }

}


void GainStage::SetTubeGain(float value)
{
    value = db2a(Blend(value,-20,20));
    // (TubeFn((value*gain-bias)+postAdd)*gainScale;
    if (value < 1E-7f)
    {
        value = 1E-7F;
    }
    this->effectiveGain = value;

    double yZero = TubeFn(-bias);
    double yMax = TubeFn(1*effectiveGain-bias);
    double yMin = TubeFn(-1*effectiveGain-bias);

    postAdd = -yZero; 
    double max = std::max( yMax+postAdd, -(yMin+postAdd));

    this->gainScale = 1.0/max;
}

float GainStage::TickSupersampled(float value)
{
    double x0 = upsamplingFilter.Tick(value);
    double x1 = upsamplingFilter.Tick(value);
    double x2 = upsamplingFilter.Tick(value);
    double x3 = upsamplingFilter.Tick(value);

    downsamplingFilter.Tick(GainFn(x0));
    downsamplingFilter.Tick(GainFn(x1));
    downsamplingFilter.Tick(GainFn(x2));
    return Undenormalize((float)downsamplingFilter.Tick(GainFn(x3)));

}
