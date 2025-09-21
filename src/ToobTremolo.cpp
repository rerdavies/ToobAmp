// Copyright (c) Robin E. R. Davies
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

#include "ToobTremolo.hpp"
#include "LsNumerics/Denorms.hpp"

#pragma GCC optimize ("O0") 
 
using namespace LsNumerics;

ToobTremolo::ToobTremolo(double rate,
                         const char *bundle_path,
                         const LV2_Feature *const *features)
    : ToobTremoloBase(rate, bundle_path, features)
{
    tremolo__lfoShape = this->MapURI("http://two-play.com/plugins/toob-tremolo#lfoShape");

    depthDezipper.SetSampleRate(rate);
    sinLfo.SetFrequency(rate);

    lowPass.SetSampleRate(rate);
    midLowPass.SetSampleRate(rate);
    midHighPass.SetSampleRate(rate);
    highPass.SetSampleRate(rate);

    // vibroverb is 144Hz/632Hz, which notches the signal between 144Hz and 632Hz.
    // we reproduce the Fender behaviour.
    static constexpr double lfC = 144;
    static constexpr double hfC = 632;

    lowPass.SetCutoffFrequency(lfC);
    // mid shelf. 
    midHighPass.SetCutoffFrequency(lfC);
    midLowPass.SetCutoffFrequency(hfC);
    highPass.SetCutoffFrequency(hfC);
}

ToobTremolo::~ToobTremolo()
{
}

void ToobTremolo::Run(uint32_t n_samples)
{
    fp_state_t fpState = disable_denorms();
    if (shape.HasChanged())
    {
        shapeMap.SetShape(shape.GetValue());
        RequestDelayedLfoShape();
    }
    if (this->depth.HasChanged())
    {
        this->depthDezipper.To(depth.GetValue(), 0.1);
    }

    if (this->rate.HasChanged())
    {
        sinLfo.SetRate(this->rate.GetValue());
    }
    if (this->inr.Get())  // stereo?
    {
        if (harmonic.GetValue())
        {
            RunHarmonicStereo(n_samples);
        }
        else
        {
            RunNormalStereo(n_samples);
        }
    } else {
        if (harmonic.GetValue())
        {
            RunHarmonicMono(n_samples);
        }
        else
        {
            RunNormalMono(n_samples);
        }

    }
    if (requestLfoShapeCount > 0)
    {
        requestLfoShapeCount -= n_samples;
        if (requestLfoShapeCount <= 0)
        {
            sendLfoShape = true;
            requestLfoShapeCount = 0;
        }
    }
    if (sendLfoShape)
    {
        sendLfoShape = false;
        requestLfoShapeCount = 0;
        WriteLfoShape();
    }
    restore_denorms(fpState);

}
void ToobTremolo::RunNormalStereo(uint32_t n_samples)
{
    const float *inL = this->inl.Get();
    const float *inR = this->inr.Get();
    float *outL = this->outl.Get();
    float *outR = this->outr.Get();

    for (size_t i = 0; i < n_samples; ++i)
    {
        float depth = depthDezipper.Tick();
        float dry = 1 - depth;
        float lfo = sinLfo.Tick();

        float volL = 0.5 + 0.5 * lfo;
        float volR = 0.5 - 0.5 * lfo; // vactrolR.tick(0.5f*(1.0f-lfo)) *depth;

        volL = shapeMap.Map(volL);
        volR = shapeMap.Map(volR);

        volL *= depth;
        volR *= depth;

        outL[i] = (dry + volL) * inL[i];
        outR[i] = (dry + volR) * inR[i];
    }
}

void ToobTremolo::RunNormalMono(uint32_t n_samples)
{
    const float *inL = this->inl.Get();
    float *outL = this->outl.Get();

    for (size_t i = 0; i < n_samples; ++i)
    {
        float depth = depthDezipper.Tick();
        float dry = 1 - depth;
        float lfo = sinLfo.Tick();

        float volL = 0.5 + 0.5 * lfo;

        volL = shapeMap.Map(volL);

        volL *= depth;

        outL[i] = (dry + volL) * inL[i];
    }
}

void ToobTremolo::RunHarmonicStereo(uint32_t n_samples)
{
    const float *inL = this->inl.Get();
    const float *inR = this->inr.Get();
    float *outL = this->outl.Get();
    float *outR = this->outr.Get();

    for (size_t i = 0; i < n_samples; ++i)
    {
        float depth = depthDezipper.Tick();
        float dry = 1 - depth;

        float lfo = sinLfo.Tick();
        float volL = 0.5f+0.5f*lfo;
        float volR = 0.5f-0.5f*lfo; // vactrolR.tick(0.5f*(1.0f-lfo)) *depth;

        volL = shapeMap.Map(volL);
        volR = shapeMap.Map(volR);

        volL *= depth;
        volR *= depth;

        float lVal = inL[i];
        float rVal = inR[i];
        outL[i] = dry * lVal + volL * lowPass.Tick(lVal) + volR * highPass.Tick(lVal);
        outR[i] = dry * rVal + volR * lowPass.TickR(rVal) + volL * highPass.TickR(rVal);
    }
}
 void ToobTremolo::RunHarmonicMono(uint32_t n_samples)
{
    const float *inL = this->inl.Get();
    float *outL = this->outl.Get();

    for (size_t i = 0; i < n_samples; ++i)
    {
        float depth = depthDezipper.Tick();
        float dry = 1 - depth;

        float lfo = sinLfo.Tick();
        float volL = 0.5f+0.5f*lfo;
        float volR = 0.5f-0.5f*lfo;

        volL = shapeMap.Map(volL);
        volR = shapeMap.Map(volR);

        volL *= depth;
        volR *= depth;

        float lVal = inL[i];
        outL[i] = dry * lVal + volL * lowPass.Tick(lVal) + volR * highPass.Tick(lVal);
    }
}

void ToobTremolo::Activate()
{
    super::Activate();
    sinLfo.SetRate(this->rate.GetValue());
    sendLfoShape = true;
    depthDezipper.To(depth.GetValue(), 0);
}
void ToobTremolo::Deactivate()
{
    super::Activate();
}

void ToobTremolo::RequestDelayedLfoShape()
{
    if (requestLfoShapeCount == 0)
    {
        requestLfoShapeCount = (int64_t)(getRate() / 15.0f);
    }
}

void ToobTremolo::OnPatchGet(LV2_URID propertyUrid)
{
    if (propertyUrid == tremolo__lfoShape)
    {
        this->sendLfoShape = true; //
    }
}

void ToobTremolo::WriteLfoShape()
{
    constexpr size_t RESPONSE_BINS = 60;

    std::array<float, RESPONSE_BINS + 4> data;
    size_t ix = 0;

    data[ix++] = 0;
    data[ix++] = 1;
    data[ix++] = 1.1;
    data[ix++] = -0.1;

    double x = -M_PI / 2;
    double dx = 3 * M_PI / RESPONSE_BINS;

    for (size_t i = 0; i < RESPONSE_BINS; ++i)
    {
        float lfo = 0.5f + 0.5f * std::sin(x);
        x += dx;
        data[ix++] = shapeMap.Map(lfo);
    }

    this->PutPatchProperty(0, tremolo__lfoShape, data.size(), data.data());
}

ToobTremoloMono::ToobTremoloMono(double rate,
                                 const char *bundle_path,
                                 const LV2_Feature *const *features)
    : super(rate, bundle_path, features)
{
}

ToobTremoloMono::~ToobTremoloMono()
{
    
}


REGISTRATION_DECLARATION PluginRegistration<ToobTremolo> toobTremoloRegistration(ToobTremolo::URI);

REGISTRATION_DECLARATION PluginRegistration<ToobTremoloMono> toobTremoloMonoRegistration(ToobTremoloMono::URI);
