#include "ToobFreeverb.h"

using namespace TwoPlay;

ToobFreeverb::ToobFreeverb(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    :   Lv2Plugin(features),
        rate(rate),
        bundle_path(bundle_path)

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
