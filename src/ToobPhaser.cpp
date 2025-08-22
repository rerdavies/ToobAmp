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

#include "ToobPhaser.hpp"
#include "restrict.hpp"

ToobPhaser::ToobPhaser(double rate,
                 const char *bundle_path,
                 const LV2_Feature *const *features)
    : ToobPhaserBase(rate,bundle_path,features),
      phaser(rate)
{
    dryWetDezipper.SetSampleRate(rate);
}

ToobPhaser::~ToobPhaser()
{
}



void ToobPhaser::Run(uint32_t n_samples) {
    const float* restrict inL = this->in.Get();
    float * restrict outL = this->out.Get();

    float rate = this->rate.GetValue();
    phaser.setLfoRate(rate);

    if (this->dryWet.HasChanged())
    {
        dryWetDezipper.To(dryWet.GetValue(),0.1f);
    }

    for (size_t i = 0; i < n_samples; ++i)
    {
        float input = inL[i];
        float output = phaser.process(input);
        float wet = dryWetDezipper.Tick();
        float dry = 1.0f-wet;
        outL[i] = dry*input+wet*output;
    }
}

void ToobPhaser::Activate() 
{
    phaser.reset();
    dryWetDezipper.To(dryWet.GetValue(),0);
}
void ToobPhaser::Deactivate() 
{

}



REGISTRATION_DECLARATION PluginRegistration<ToobPhaser> toobPhaserRegistration(ToobPhaser::URI);
