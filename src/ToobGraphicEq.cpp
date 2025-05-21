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
#include "ControlDezipper.h"

using namespace graphiceq_plugin;

ToobGraphicEq::ToobGraphicEq(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    : ToobGraphicEqBase(rate, bundle_path, features),
      graphicEq(rate,7,100,2)
{
    bandInputPorts = std::vector<RangedDbInputPort *>{
        &gain_100hz,
        &gain_200hz,
        &gain_400hz,
        &gain_800hz,
        &gain_1600hz,
        &gain_3200hz,
        &gain_6400hz};
}

ToobGraphicEq::~ToobGraphicEq()
{
}

void ToobGraphicEq::Run(uint32_t n_samples)
{
    for (size_t i = 0; i < bandDezippers.size(); ++i)
    {
        auto & dezipper = bandDezippers[i];
        if (bandInputPorts[i]->HasChanged())
        {
            dezipper.SetTarget(bandInputPorts[i]->GetDbNoLimit());
        }
    }
    if (level.HasChanged()) {
        levelDezipper.SetTarget(level.GetDbNoLimit());
    }

    graphicEq.setLevel(this->level.GetAfNoLimit());

    const float *inL = in_left.Get();
    float *outL = out_left.Get();
    for (size_t i = 0; i < n_samples; ++i)
    {
        for (size_t z = 0; z < bandDezippers.size(); ++z)
        {
            auto & dezipper = bandDezippers[z];
            if (!dezipper.IsIdle()) {
                graphicEq.setGain(z,dezipper.Tick());
            }
        }
        double y = graphicEq.process(inL[i]); 
        outL[i] = y * levelDezipper.Tick();
    }
}

void ToobGraphicEq::Activate()
{
    levelDezipper.SetSampleRate(this->getRate());
    levelDezipper.SetRate(0.1);
    levelDezipper.Reset(level.GetDbNoLimit());

    bandDezippers.resize(bandInputPorts.size());
    for (size_t i = 0; i < bandDezippers.size(); ++i)
    {
        auto & dezipper = bandDezippers[i];
        dezipper.SetSampleRate(this->getRate());
        dezipper.SetRate(0.1);
        dezipper.Reset(bandInputPorts[i]->GetDbNoLimit());
        graphicEq.setGain(i, dezipper.Tick());
    }
    graphicEq.reset();
}
void ToobGraphicEq::Deactivate()
{
}

REGISTRATION_DECLARATION PluginRegistration<ToobGraphicEq> toobGraphicEqRegistration(ToobGraphicEq::URI);
