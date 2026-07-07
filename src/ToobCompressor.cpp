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


REGISTRATION_DECLARATION PluginRegistration<ToobCompressor> toobCompressorRegistration(ToobCompressor::URI);
