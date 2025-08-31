/*
 *   Copyright (c) 2025 Robin E. R. Davies
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

// ToobParametricEq.cpp : Defines the entry point for the application.
//

#include "ToobParametricEq.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;
using namespace toob;
using namespace LsNumerics;

#ifndef _MSC_VER
#include <unistd.h>
#include <signal.h>
#include <csignal>
#endif


const int MAX_UPDATES_PER_SECOND = 10;


uint64_t timeMs();


ToobParametricEq::ToobParametricEq(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	super(_rate, _bundle_path,features),
	rate(_rate),
	filterResponse()
{
	uris.Map(this);
    eq.SetSampleRate(rate);
	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND);
	this->updateMsDelay = (1000/MAX_UPDATES_PER_SECOND);
    gainDezipper.SetSampleRate(_rate);
    gainDezipper.SetRate(0.1f); // 100ms dezipper time.
}

ToobParametricEq::~ToobParametricEq()
{

}


void ToobParametricEq::Activate()
{
	super::Activate();
	responseChanged = true;
	frameTime = 0;
    gainDezipper.Reset(gain.GetDb());

    {
        float value = loCut.GetValue();
        if (value == loCut.GetMinValue())
        {
            eq.lowCut.Disable();
        } else {
            eq.lowCut.SetCutoffFrequency(this->loCut.GetValue());
        }

    }
    {
        float value = this->hiCut.GetValue();
        if (value == this->hiCut.GetMaxValue())
        {
            eq.highCut.Disable();
        } else {
            eq.highCut.SetCutoffFrequency(this->hiCut.GetValue()*1000.0f);
        }
    }


    eq.lowShelf.SetLowShelf(this->lfLevel.GetDb(),this->lfC.GetValue());
    eq.highShelf.SetHighShelf(this->hfLevel.GetDb(),this->hfC.GetValue()*1000.0f);
    eq.lmf.Set(this->lmfC.GetValue(),this->lmfLevel.GetDb(),this->lmfQ.GetValue());
    eq.hmf.Set(this->hmfC.GetValue()*1000.0f,this->hmfLevel.GetDb(),this->hmfQ.GetValue());

    this->responseChanged = true;
}
void ToobParametricEq::Deactivate()
{
}

void ToobParametricEq::Run(uint32_t n_samples)
{

	if (UpdateControls())
	{
		this->responseChanged = true;
	}
    const float *input = this->in.Get();
    float *output =  this->out.Get();

    for (size_t i = 0; i < n_samples; ++i)
    {
        output[i] = this->eq.Tick(input[i]);
    }


    for (size_t i = 0; i < n_samples; ++i)
    {
        output[i] *= gainDezipper.Tick();
    }

	frameTime += n_samples;


	if (responseChanged)
	{
		responseChanged = false;
		// delay by samples or ms, depending on whether we're connected.
		if (n_samples == 0)
		{
			updateMs = timeMs() + this->updateMsDelay;
		} else {
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

}

bool ToobParametricEq::UpdateControls()
{
    if (gain.HasChanged())
    {
        float gain = this->gain.GetDb();
        gainDezipper.SetTarget(gain);
    }
    bool changed = false;
    if (this->loCut.HasChanged())
    {
        float value = loCut.GetValue();
        if (value == loCut.GetMinValue())
        {
            eq.lowCut.Disable();
        } else {
            eq.lowCut.SetCutoffFrequency(this->loCut.GetValue());
        }
        changed = true;

    }
    if (this->hiCut.HasChanged())
    {
        float value = this->hiCut.GetValue();
        if (value == this->hiCut.GetMaxValue())
        {
            eq.highCut.Disable();
        } else {
            eq.highCut.SetCutoffFrequency(this->hiCut.GetValue()*1000.0);
        }
        changed = true;
    }
    if (this->lfLevel.HasChanged() || this->lfC.HasChanged())
    {
        eq.lowShelf.SetLowShelf(this->lfLevel.GetDb(),this->lfC.GetValue());
        changed = true;
    }
    if (this->hfLevel.HasChanged() || this->hfC.HasChanged())
    {
        eq.highShelf.SetHighShelf(this->hfLevel.GetDb(),this->hfC.GetValue()*1000);
        changed = true;
    }
    if (this->lmfLevel.HasChanged() || this->lmfC.HasChanged() || this->lmfQ.HasChanged())
    {
        eq.lmf.Set(this->lmfC.GetValue(),this->lmfLevel.GetDb(),this->lmfQ.GetValue());
        changed = true;
    }
    if (this->hmfLevel.HasChanged() || this->hmfC.HasChanged() || this->hmfQ.HasChanged())
    {
        eq.hmf.Set(this->hmfC.GetValue()*1000.0,this->hmfLevel.GetDb(),this->hmfQ.GetValue());
        changed = true;
    }
	return changed;
}

float ToobParametricEq::CalculateFrequencyResponse(float f)
{
    return eq.GetFrequencyResponse(f);
}


void ToobParametricEq::WriteFrequencyResponse()
{
    LV2_Atom_Forge &forge = super::outputForge;

	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		filterResponse.SetResponse(
			i,
			this->CalculateFrequencyResponse(
				filterResponse.GetFrequency(i)
				));
	}


	lv2_atom_forge_frame_time(&forge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;
	lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&forge, uris.patch__property);		
	lv2_atom_forge_urid(&forge, uris.param_frequencyResponseVector);
	lv2_atom_forge_key(&forge, uris.patch__value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__Float);
	
    lv2_atom_forge_float(&forge,30.0f);
    lv2_atom_forge_float(&forge,20000.0f);
    lv2_atom_forge_float(&forge,20.0f);
    lv2_atom_forge_float(&forge,-20.0f);

	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		//lv2_atom_forge_float(&forge,filterResponse.GetFrequency(i));
		lv2_atom_forge_float(&forge,filterResponse.GetResponse(i));
	}
	lv2_atom_forge_pop(&forge, &vectorFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);
}


void ToobParametricEq::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == uris.param_frequencyResponseVector)
	{
        this->patchGet = true; // 
	}

}

REGISTRATION_DECLARATION PluginRegistration<ToobParametricEq> toobParametricEqRegistration(ToobParametricEq::URI);
