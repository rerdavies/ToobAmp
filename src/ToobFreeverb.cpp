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
#include "ToobFreeverb.h"
#include "LsNumerics/Denorms.hpp"

using namespace toob;
using namespace LsNumerics;

ToobFreeverb::ToobFreeverb(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2Plugin(rate, bundle_path, features),
      freeverb(rate),
      rate(rate),
      bundle_path(bundle_path)
{
}

const char *ToobFreeverb::URI = TOOB_FREEVERB_URI;

void ToobFreeverb::ConnectPort(uint32_t port, void *data)
{
    switch ((PortId)port)
    {
    case PortId::BYPASS:
        this->bypass = (const float *)data;
        break;
    case PortId::TAILS:
        this->tails = (const float *)data;
        break;
    case PortId::DRYWET:
        this->dryWet = (const float *)data;
        break;
    case PortId::ROOMSIZE:
        this->roomSize = (const float *)data;
        break;
    case PortId::DAMPING:
        this->damping = (const float *)data;
        break;
    case PortId::AUDIO_INL:
        this->inL = (const float *)data;
        break;
    case PortId::AUDIO_INR:
        this->inR = (const float *)data;
        break;
    case PortId::AUDIO_OUTL:
        this->outL = (float *)data;
        break;
    case PortId::AUDIO_OUTR:
        this->outR = (float *)data;
        break;
    }
}
void ToobFreeverb::Activate()
{
    dryWetValue = *dryWet;
    freeverb.setEffectMix(dryWetValue);

    roomSizeValue = *roomSize;
    freeverb.setRoomSize(roomSizeValue);

    dampingValue = *damping;
    freeverb.setDamping(dampingValue);

    bypassValue = *bypass != 0;
    tailsValue = *tails != 0;

    freeverb.setTails(*tails != 0);
    freeverb.setBypass(bypassValue,true);


    freeverb.clear();
}
void ToobFreeverb::Run(uint32_t n_samples)
{
    fp_state_t oldstate = disable_denorms();

    if (dryWetValue != *dryWet)
    {
        dryWetValue = *dryWet;
        freeverb.setEffectMix(dryWetValue);
    }

    if (roomSizeValue != *roomSize)
    {
        roomSizeValue = *roomSize;
        freeverb.setRoomSize(roomSizeValue);
    }

    if (dampingValue != *damping)
    {
        dampingValue = *damping;
        freeverb.setDamping(dampingValue);
    }
    freeverb.setTails(*tails != 0);

    bool t = *bypass != 0;
    if (bypassValue != t) 
    {
        bypassValue = t;
        freeverb.setBypass(t,false);
    }

    for (uint32_t i = 0; i < n_samples; ++i)
    {
        freeverb.tick(inL[i], inR[i], &outL[i], &outR[i]);
    }
    restore_denorms(oldstate);
}
void ToobFreeverb::Deactivate()
{
}
