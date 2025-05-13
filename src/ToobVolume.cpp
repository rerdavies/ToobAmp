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

#include "ToobVolume.hpp"

ToobVolume::ToobVolume(double rate,
                 const char *bundle_path,
                 const LV2_Feature *const *features)
    : ToobVolumeBase(rate,bundle_path,features)
{
    dezipVol.SetSampleRate(rate);
}

ToobVolume::~ToobVolume()
{
}


void ToobVolume::Mix(uint32_t n_samples)
{
    const float* in = this->in.Get();
    float *out = this->out.Get();
    float vol = this->vol.GetAf();
    constexpr float DEZIP_DELAY_S = 0.1f;
    dezipVol.To(vol,DEZIP_DELAY_S);

    for (size_t i = 0; i < n_samples; ++i)
    {
        out[i] = dezipVol.Tick() *in[i];
    }
}

void ToobVolume::Run(uint32_t n_samples) {
    Mix(n_samples);
}

void ToobVolume::Activate() 
{
    super::Activate();
    float vol = this->vol.GetAf();
    constexpr float DEZIP_DELAY_S = 0.1f;
    dezipVol.To(vol,DEZIP_DELAY_S);

}
void ToobVolume::Deactivate() 
{
    super::Deactivate();
}



REGISTRATION_DECLARATION PluginRegistration<ToobVolume> toobVolumeRegistration(ToobVolume::URI);
