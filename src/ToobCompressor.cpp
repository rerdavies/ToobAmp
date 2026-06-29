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

#include "ToobCompressor.hpp"

ToobCompressor::ToobCompressor(double rate,
                 const char *bundle_path,
                 const LV2_Feature *const *features)
    : ToobCompressorBase(rate, bundle_path, features)
{
    compressor.Initialize(rate);
}

ToobCompressor::~ToobCompressor()
{
}

static inline void applyPan(float pan, float vol, float &left, float &right, float phase)
{
    // hard pan law.
    if (pan < 0)
    {
        left = vol * 1.0f * phase;
        right = vol * (1.0f + pan) * phase;
    }
    else
    {
        left = vol * (1.0 - pan) * phase;
        right = vol * 1.0f * phase;
    }
}

void ToobCompressor::Compressor(uint32_t n_samples)
{
    const float*inL = this->inL.Get();
    const float *inR = this->inR.Get();
    if (inR) {
        // stereo.
        // float*outL = this->outL.Get();
        // float *outR = this->outR.Get();
    } else {
        float*outL = this->outL.Get();
        for (size_t i = 0; i < n_samples; ++i)
        {
            outL[i] = (float)compressor.Tick(inL[i]);
        }
    }
}

void ToobCompressor::Run(uint32_t n_samples)
{
    Compressor(n_samples);
}

void ToobCompressor::Activate()
{
    compressor.Reset();
}
void ToobCompressor::Deactivate()
{
}

inline float CompressorChannel::Tick(float value)
{
    double v2 = lowCut.Tick(value);
    double v3 = lowCut2.Tick(v2);
    double v4 = highShelf2.Tick(v3);

    return v4;
}
inline StereoResult CompressorChannel::Tick(float left, float right)
{
    StereoResult v2 { (float)lowCut.Tick(left), (float)lowCut.TickR(right)};
    return v2;
}


void CompressorChannel::Initialize(double sampleRate)
{
    lowCut.SetSampleRate(sampleRate);
    lowCut.SetCutoffFrequency(3.1);    
    lowCut2.SetSampleRate(sampleRate);
    lowCut.SetCutoffFrequency(8.0);    
    highShelf2.SetSampleRate(sampleRate);
    highShelf2.SetHighShelf(24,3000);
}

void CompressorChannel::Reset() {
    lowCut.Reset();
}

REGISTRATION_DECLARATION PluginRegistration<ToobCompressor> toobCompressorRegistration(ToobCompressor::URI);
