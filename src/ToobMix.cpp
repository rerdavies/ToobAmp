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

#include "ToobMix.hpp"

ToobMix::ToobMix(double rate,
                 const char *bundle_path,
                 const LV2_Feature *const *features)
    : ToobMixBase(rate,bundle_path,features)
{
    zipLL.SetSampleRate(rate);
    zipLR.SetSampleRate(rate);
    zipRL.SetSampleRate(rate);
    zipRR.SetSampleRate(rate);
}

ToobMix::~ToobMix()
{
}

static void applyPan(float pan, float vol, float&left,float&right)
{
    // hard pan law.
    if (pan < 0) {
        left = vol*1.0f; 
        right =  vol * (1.0f+pan);
    } else {
        left = vol * (1.0-pan);
        right = vol*1.0f;
    }
}

void ToobMix::Mix(uint32_t n_samples)
{
    const float* inL = this->inl.Get();
    const float* inR = this->inr.Get();
    float *outL = this->outl.Get();
    float *outR = this->outr.Get();

    float ll, lr;
    float rl, rr;
    applyPan(this->panL.GetValue(),this->trimL.GetAf(),ll,lr);
    applyPan(this->panR.GetValue(),this->trimR.GetAf(),rl,rr);
    constexpr float DEZIP_DELAY_S = 0.1; // tick < 10hz.
    zipLL.To(ll,DEZIP_DELAY_S);
    zipLR.To(lr,DEZIP_DELAY_S);
    zipRL.To(rl,DEZIP_DELAY_S);
    zipRR.To(rr,DEZIP_DELAY_S);

    for (size_t i = 0; i < n_samples; ++i)
    {
        outL[i] = zipLL.Tick() *inL[i] + zipRL.Tick()*inR[i];
        outR[i] = zipLR.Tick()*inL[i] + zipRR.Tick()*inR[i];
    }
}

void ToobMix::Run(uint32_t n_samples) {
    Mix(n_samples);
}

void ToobMix::Activate() 
{
    float ll, lr;
    float rl, rr;
    applyPan(this->panL.GetValue(),this->trimL.GetAf(),ll,lr);
    applyPan(this->panR.GetValue(),this->trimR.GetAf(),rl,rr);
    constexpr float DEZIP_DELAY_S = 0.0; // immediate.
    zipLL.To(ll,DEZIP_DELAY_S);
    zipLR.To(lr,DEZIP_DELAY_S);
    zipRL.To(rl,DEZIP_DELAY_S);
    zipRR.To(rr,DEZIP_DELAY_S);

}
void ToobMix::Deactivate() 
{

}



REGISTRATION_DECLARATION PluginRegistration<ToobMix> toobMixRegistration(ToobMix::URI);
