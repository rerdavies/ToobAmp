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
#include "ToobChorus.h"




#define TOOB_CHORUS_URI "http://two-play.com/plugins/toob-chorus"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif


using namespace toob;


ToobChorus::ToobChorus(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2Plugin(rate,bundle_path,features),
      rate(rate),
      bundle_path(bundle_path),
      chorus(rate)

{
}

const char *ToobChorus::URI = TOOB_CHORUS_URI;

void ToobChorus::ConnectPort(uint32_t port, void *data)
{
    switch ((PortId)port)
    {
    case PortId::RATE:
        this->pRate = (float *)data;
        break;
    case PortId::DEPTH:
        this->pDepth = (float *)data;
        break;
    case PortId::DRYWET:
        this->pDryWet = (const float*)data;
        break;
    case PortId::AUDIO_INL:
        this->inL = (const float *)data;
        break;
    case PortId::AUDIO_OUTL:
        this->outL = (float *)data;
        break;
    case PortId::AUDIO_OUTR:
        this->outR = (float *)data;
        break;
    }
}
void ToobChorus::clear()
{
    chorus.Clear();
}
inline void ToobChorus::updateControls()
{
    if (lastRate != *pRate)
    {
        lastRate = *pRate;
        double value = lastRate;
        if (value < 0) value = 0;
        if (value > 1) value = 1;
        chorus.SetRate(value);
    }
    if (lastDepth != *pDepth)
    {
        lastDepth = *pDepth;
        double value = lastDepth;
        if (value < 0) value = 0;
        if (value > 1) value = 1;
        chorus.SetDepth(value);

    }
    if (lastDryWet != *pDryWet)
    {
        lastDryWet = *pDryWet;
        dryWetDezipper.To(lastDryWet,0.1f);
    }
}
void ToobChorus::Activate()
{
    lastRate = lastDepth = -1E30; // force updates
    updateControls();
    dryWetDezipper.To(lastDryWet,0);
    clear();
}

void ToobChorus::Run(uint32_t n_samples)
{
    updateControls();
    if (this->outR != nullptr)
    {
        for (uint32_t i = 0; i < n_samples; ++i)
        {
            float wet = dryWetDezipper.Tick();
            float dry = 1.0-wet;
            float l,r;
            float input = inL[i];
            chorus.Tick(inL[i],&l,&r);
            outL[i] = input*dry+l*wet;
            outR[i] = input*dry+r*wet;
        }
    } else {
        for (uint32_t i = 0; i < n_samples; ++i)
        {
            float input = inL[i];

            float output = chorus.Tick(input);
            float wet = dryWetDezipper.Tick();
            float dry = 1.0f-wet;
            
            outL[i] = dry*input+wet*output;
        }
    }
}
void ToobChorus::Deactivate()
{
}
