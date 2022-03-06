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

using namespace TwoPlay;

ToobFreeverb::ToobFreeverb(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    :   Lv2Plugin(features),
        rate(rate),
        bundle_path(bundle_path),
        freeverb(rate)
        

{
}

const char *ToobFreeverb::URI = TOOB_FREEVERB_URI;

void ToobFreeverb::ConnectPort(uint32_t port, void *data)
{
    switch ((PortId)port)
    {
    case PortId::DRYWET:
        this->dryWet = (float *)data;
        break;
    case PortId::ROOMSIZE:
        this->roomSize = (float *)data;
        break;
    case PortId::DAMPING:
        this->damping = (float *)data;
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
    dryWetValue + *dryWet;
    freeverb.setEffectMix(dryWetValue);

    roomSizeValue = *roomSize;
    freeverb.setRoomSize(roomSizeValue);

    dampingValue = *damping;
    freeverb.setDamping(dampingValue);

    freeverb.clear();
}
void ToobFreeverb::Run(uint32_t n_samples)
{
    for (uint32_t i = 0; i < n_samples; ++i)
    {
        freeverb.tick(inL[i],inR[i],&outL[i],&outR[i]);
    }
}
void ToobFreeverb::Deactivate()
{

}
