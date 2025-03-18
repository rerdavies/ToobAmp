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
#include "ToobFlanger.h"




#define TOOB_FLANGER_URI "http://two-play.com/plugins/toob-flanger"
#define TOOB_FLANGER_STEREO_URI "http://two-play.com/plugins/toob-flanger-stereo"


#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif


using namespace toob;


ToobFlangerBase::ToobFlangerBase(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2Plugin(rate,bundle_path,features),
      rate(rate),
      bundle_path(bundle_path),
      flanger(rate)

{
}

const char *ToobFlanger::URI = TOOB_FLANGER_URI;
const char *ToobFlangerStereo::URI = TOOB_FLANGER_STEREO_URI;

void ToobFlangerBase::ConnectPort(uint32_t port, void *data)
{
    switch ((PortId)port)
    {
    case PortId::MANUAL:
        this->pManual = (const float*)data;
        break;
    case PortId::RES:
        this->pRes = (const float*)data;
        break;
    case PortId::RATE:
        this->pRate = (const float *)data;
        break;
    case PortId::LFO:
        this->pLfo = (float*)data;
        break;
    case PortId::DEPTH:
        this->pDepth = (const float *)data;
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
void ToobFlangerBase::clear()
{
    flanger.Clear();
}
inline void ToobFlangerBase::updateControls()
{
    if (lastManual != *pManual)
    {
        lastManual = *pManual;
        double value = lastManual;
        if (value < 0) value = 0;
        if (value > 1) value = 1;

        flanger.SetManual(value);
    }
    if (lastRes != *pRes)
    {
        lastRes = *pRes;
        double value = lastRes;
        if (value < 0) value = 0;
        if (value > 1) value = 1;

        flanger.SetRes(value);
    }

    if (lastRate != *pRate)
    {
        lastRate = *pRate;
        double value = lastRate;
        if (value < 0) value = 0;
        if (value > 1) value = 1;
        flanger.SetRate(value);
    }
    if (lastDepth != *pDepth)
    {
        lastDepth = *pDepth;
        double value = lastDepth;
        if (value < 0) value = 0;
        if (value > 1) value = 1;
        flanger.SetDepth(value);

    }
}
void ToobFlangerBase::Activate()
{
    lastRate = lastDepth = -1E30; // force updates
    updateControls();
    clear();
}

void ToobFlangerBase::Run(uint32_t n_samples)
{
    updateControls();
    if (outR != nullptr)
    {
        for (uint32_t i = 0; i < n_samples; ++i)
        {
            float input = inL[i];

            flanger.Tick(input,&(outL[i]),&(outR[i]));
        }
    } else {
        for (uint32_t i = 0; i < n_samples; ++i)
        {
            float input = inL[i];

            outL[i] = flanger.Tick(input);
        }

    }
    *pLfo = flanger.GetLfoValue();
}
void ToobFlangerBase::Deactivate()
{
}
