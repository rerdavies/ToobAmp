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

#include "ToobGraphicEq.hpp"

using namespace graphiceq_plugin;

ToobGraphicEq::ToobGraphicEq(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    : ToobGraphicEqBase(rate, bundle_path, features),
      graphicEq(rate)
{
}

ToobGraphicEq::~ToobGraphicEq()
{
}

void ToobGraphicEq::Run(uint32_t n_samples)
{
    graphicEq.setGain(0,this->gain_100hz.GetAfNoLimit());
    graphicEq.setGain(1,this->gain_200hz.GetAfNoLimit());
    graphicEq.setGain(2,this->gain_400hz.GetAfNoLimit());
    graphicEq.setGain(3,this->gain_800hz.GetAfNoLimit());
    graphicEq.setGain(4,this->gain_1600hz.GetAfNoLimit());
    graphicEq.setGain(5,this->gain_3200hz.GetAfNoLimit());
    graphicEq.setGain(6,this->gain_6400hz.GetAfNoLimit());
    graphicEq.setLevel(this->level.GetAfNoLimit());

    graphicEq.process(in_left.Get(),out_left.Get(),n_samples);
    
    float *outR = out_right.Get();
    float *outL = out_left.Get();
    for (uint32_t i = 0; i < n_samples; ++i)
    {
        outR[i] = outL[i];
    }
}


void ToobGraphicEq::Activate()
{
    graphicEq.setGain(0,this->gain_100hz.GetAfNoLimit());
    graphicEq.setGain(1,this->gain_200hz.GetAfNoLimit());
    graphicEq.setGain(2,this->gain_400hz.GetAfNoLimit());
    graphicEq.setGain(3,this->gain_800hz.GetAfNoLimit());
    graphicEq.setGain(4,this->gain_1600hz.GetAfNoLimit());
    graphicEq.setGain(5,this->gain_3200hz.GetAfNoLimit());
    graphicEq.setGain(6,this->gain_6400hz.GetAfNoLimit());


    graphicEq.reset();

}
void ToobGraphicEq::Deactivate()
{
}

REGISTRATION_DECLARATION PluginRegistration<ToobGraphicEq> toobGraphicEqRegistration(ToobGraphicEq::URI);
