#include "std.h"

#include "GainStage.h"
#include <cmath>


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


// Atan approximating valid in range -1 to 1.
static inline double AtanApprox(double x)
{
    double x2 = x*x;

    return ((((((((0.00286623*x2-0.0161657)
                *x2+0.0429096)
                *x2-0.0752896)
                *x2+0.106563)
                *x2-0.142089)
                *x2+0.199936)
                *x2-0.333331)
                *x2+1)*x;

};

static inline double Atan(double value)
{
    if (value > 1)
    {
        return (M_PI/2)- AtanApprox(1/value);
    } else if (value < -1)
    {
        return (-M_PI/2)-AtanApprox(1/value);
    }
    else {
        return AtanApprox(value);
    }
}

static inline double TubeAFn(double value)
{
    if (value >= 1) return 1;
    double t = 1-value;
    return 1-t*std::sqrt(t);
}

static inline double TubeFn(double value)
{
    if (value > 0) 
    {
        return TubeAFn(value);
    } else {
        return -TubeAFn(-value);
    }
}

inline double GainStage::GainFn(double value)
{
    if (shape == EShape::ATAN)
        return Atan(value*gain)*gainScale;
    else 
        return TubeFn(value*tubeMIn+tubeCIn)*tubeMOut + tubeCOut;
}

void GainStage::SetShape(GainStage::EShape shape)
{
    this->shape = shape;
    SetGain(this->lastGainIn);
}
void GainStage::SetGain(float value)
{
    lastGainIn = value;
    switch (this->shape)
    {
        case EShape::ATAN:
            value = value*value;
            value = value*value*1000;
            if (value < 1E-7f)
            {
                value = 1E-7F;
            }
            this->gain = value;
            this->gainScale = 1.0/Atan(gain);
            break;
        case EShape::TUBE_CLASSA:
            SetClassAGain(value);
            break;
        case EShape::TUBE_CLASSB:
            SetClassBGain(value);
            break;
    }

}


void GainStage::SetClassAGain(float value)
{
    if (value < 1E-7f)
    {
        value = 1E-7F;
    }
    this->tubeMIn = value*0.5;
    this->tubeCIn = 0.5;
    double minOut = TubeAFn(-1*tubeMIn+tubeCIn);
    double zOut = TubeAFn(0*tubeMIn+tubeCIn);
    double maxOut = TubeAFn(1*tubeMIn+tubeCIn);
    this->tubeMOut = 1/(minOut-zOut);
    this->tubeCOut = -zOut*this->tubeMOut;
}
void GainStage::SetClassBGain(float value)
{
    value = value*value;
    value = value*value;
    if (value < 1E-7f)
    {
        value = 1E-7F;
    }
    this->tubeMIn = value;
    this->tubeCIn = 0;
    this->tubeMOut = 1/(TubeAFn(value));
    this->tubeCOut = 0;
}


float GainStage::Tick(float value)
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
