// Copyright (c) 2026 Robin E. R. Davies
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

#include "ToobTone.hpp"

static constexpr int MAX_UPDATES_PER_SECOND = 10;

uint64_t timeMs();

ToobTone::ToobTone(double rate,
                   const char *bundle_path,
                   const LV2_Feature *const *features)
    : super(rate, bundle_path, features),
      _rate(rate)
{
    uris.Map(this);

    shelvingFilter.SetSampleRate(rate);

    this->updateSampleDelay = (int)(_rate / MAX_UPDATES_PER_SECOND);
    this->updateMsDelay = (1000 / MAX_UPDATES_PER_SECOND);

    gainDezipper.SetSampleRate(rate);
    gainDezipper.Reset(0);
    gainDezipper.SetRate(0.1);
}

ToobTone::~ToobTone()
{
}

void ToobTone::UpdateGain()
{
    gainDezipper.SetTarget(gain.GetDb());
    responseChanged = true;
}
void ToobTone::UpdateFilter()
{
    float value = tone.GetValue();
    if (value < 0)
    {
        constexpr float LOW_BOOST = 20;
        float db = value * LOW_BOOST;
        shelvingFilter.SetLowShelf(-db, this->lowFc.GetValue());
        this->filterGainDb = -db * (this->lowBoost.GetDb() / LOW_BOOST);
        this->filterGain = Db2AF(filterGainDb, -120);
    }
    else
    {
        constexpr float HIGH_BOOST = 15;
        float db = value * HIGH_BOOST;
        shelvingFilter.SetHighShelf(db, this->highFc.GetValue());
        this->filterGainDb = db * (this->highBoost.GetDb() / HIGH_BOOST);
        this->filterGain = Db2AF(filterGainDb, -120);
    }
    if (isStereo)
    {
        shelvingFilterR.CopyFilterValues(shelvingFilter);
    }
    responseChanged = true;
}

void ToobTone::Run(uint32_t n_samples)
{
    const float *in = this->in.Get();
    float *out = this->out.Get();

    // prepare forge to write to notify output port.
    // Set up forge to write directly to notify output port.
    const uint32_t notify_capacity = this->notifyOut.Get()->atom.size;
    lv2_atom_forge_set_buffer(
        &(this->forge), (uint8_t *)(this->notifyOut.Get()), notify_capacity);

    // Start a sequence in the notify output port.
    LV2_Atom_Forge_Frame out_frame;

    lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.units__frame);

    HandleEvents(this->controlIn.Get());

    if (this->tone.HasChanged() || 
        this->lowFc.HasChanged() || this->lowBoost.HasChanged() ||
        this->highFc.HasChanged() || this->highBoost.HasChanged()

    )
    {
        UpdateFilter();
    }
    if (this->gain.HasChanged())
    {
        UpdateGain();
    }

    if (isStereo)
    {
        const float *inR = this->inR.Get();
        float *outR = this->outR.Get();
        for (size_t i = 0; i < n_samples; ++i)
        {
            float gain = filterGain * gainDezipper.Tick();
            out[i] = shelvingFilter.Tick(in[i]) * gain;
            outR[i] = shelvingFilter.Tick(inR[i]) * gain;
        }
    }
    else
    {
        for (size_t i = 0; i < n_samples; ++i)
        {
            out[i] = shelvingFilter.Tick(in[i]) * filterGain * gainDezipper.Tick();
        }
    }

    frameTime += n_samples;

    if (responseChanged)
    {
        responseChanged = false;
        // delay by samples or ms, depending on whether we're connected.
        if (n_samples == 0)
        {
            updateMs = timeMs() + this->updateMsDelay;
        }
        else
        {
            this->updateSamples = this->updateSampleDelay;
        }
    }
    if (this->updateSamples != 0)
    {
        this->updateSamples -= n_samples;
        if (this->updateSamples <= 0 || n_samples == 0)
        {
            updateSamples = 0;
            this->patchGet = true;
        }
    }
    if (this->updateMs != 0 && n_samples == 0)
    {
        uint64_t ctime = timeMs();
        if (ctime > this->updateMs || n_samples != 0)
        {
            updateMs = 0;
            this->patchGet = true;
        }
    }
    if (this->patchGet)
    {
        this->patchGet = false;
        this->updateSamples = 0;
        this->updateMs = 0;
        WriteFrequencyResponse();
    }
    lv2_atom_forge_pop(&forge, &out_frame);
}

void ToobTone::Activate()
{
    super::Activate();
    isStereo = this->outR.Get() != nullptr;
    UpdateFilter();
    gainDezipper.Reset(gain.GetDb());
}
void ToobTone::Deactivate()
{
    super::Deactivate();
}

inline float ToobTone::CalculateFrequencyResponse(float f)
{
    return shelvingFilter.GetFrequencyResponse(f) * filterGain;
}

void ToobTone::WriteFrequencyResponse()
{
    LV2_Atom_Forge &forge = super::outputForge;

    for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
    {
        filterResponse.SetResponse(
            i,
            this->CalculateFrequencyResponse(
                filterResponse.GetFrequency(i)));
    }

    lv2_atom_forge_frame_time(&forge, frameTime);

    LV2_Atom_Forge_Frame objectFrame;
    lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&forge, uris.patch__property);
    lv2_atom_forge_urid(&forge, uris.param_frequencyResponseVector);
    lv2_atom_forge_key(&forge, uris.patch__value);

    LV2_Atom_Forge_Frame vectorFrame;
    lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__Float);

    lv2_atom_forge_float(&forge, 30.0f);
    lv2_atom_forge_float(&forge, 20000.0f);
    lv2_atom_forge_float(&forge, 20.0f);
    lv2_atom_forge_float(&forge, -20.0f);

    for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
    {
        // lv2_atom_forge_float(&forge,filterResponse.GetFrequency(i));
        lv2_atom_forge_float(&forge, filterResponse.GetResponse(i));
    }
    lv2_atom_forge_pop(&forge, &vectorFrame);

    lv2_atom_forge_pop(&forge, &objectFrame);
}

void ToobTone::OnPatchGet(LV2_URID propertyUrid)
{
    if (propertyUrid == uris.param_frequencyResponseVector)
    {
        this->patchGet = true; //
    }
}

REGISTRATION_DECLARATION PluginRegistration<ToobTone> toobToneRegistration(ToobTone::URI);

REGISTRATION_DECLARATION PluginRegistration<ToobTone> toobToneStereoRegistration(ToobTone::STEREO_URI);
