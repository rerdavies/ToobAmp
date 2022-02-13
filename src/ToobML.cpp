/*
 *   Copyright (c) 2021 Robin E. R. Davies
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

// ToobML.cpp : Defines the entry point for the application.
//

#include "ToobML.h"

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
using namespace TwoPlay;

#ifndef _MSC_VER
#include <unistd.h>
#include <signal.h>
#include <csignal>
#endif


const int MAX_UPDATES_PER_SECOND = 10;

const char* ToobML::URI= TOOB_ML_URI;



uint64_t timeMs();


ToobML::ToobML(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(features),
	rate(_rate),
	filterResponse(),
	bundle_path(_bundle_path),
	programNumber(0)
{
	uris.Map(this);
	lv2_atom_forge_init(&forge, map);
	this->toneStackFilter.SetSampleRate(_rate);

	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND);
	this->updateMsDelay = (1000/MAX_UPDATES_PER_SECOND);
}

ToobML::~ToobML()
{

}

void ToobML::ConnectPort(uint32_t port, void* data)
{
	switch ((PortId)port) {

	case PortId::TRIM:
		this->trimData  = (const float*)data;
		break;
	case PortId::MASTER:
		this->masterData = (const float*)data;
		break;
	case PortId::BASS:
		toneStackFilter.Bass.SetData(data);
		break;
	case PortId::MID:
		toneStackFilter.Mid.SetData(data);
		break;
	case PortId::TREBLE:
		toneStackFilter.Treble.SetData(data);
		break;
	case PortId::AMP_MODEL:
		toneStackFilter.AmpModel.SetData(data);
		break;
	case PortId::AUDIO_IN:
		this->input = (const float*)data;
		break;

	case PortId::AUDIO_OUT:
		this->output = (float*)data;
		break;
	case PortId::CONTROL_IN:
		this->controlIn = (LV2_Atom_Sequence*)data;
		break;
	case PortId::NOTIFY_OUT:
		this->notifyOut = (LV2_Atom_Sequence*)data;
		break;
	}
}

void ToobML::Activate()
{
	
	responseChanged = true;
	frameTime = 0;
	this->toneStackFilter.Reset();
}
void ToobML::Deactivate()
{
}

void ToobML::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t*)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.unitsFrame);


	HandleEvents(this->controlIn);

	if (toneStackFilter.UpdateControls())
	{
		this->responseChanged = true;
	}

	if (*trimData != trimDb)
	{
		trimDb = *trimData;
		trim = Db2Af(trimDb);
	}
	if (*masterData != masterDb)
	{
		masterDb = *masterData;
		master = Db2Af(masterDb);
	}


	for (uint32_t i = 0; i < n_samples; ++i)
	{
		float in = trim*input[i];
		float val = Undenormalize((float)toneStackFilter.Tick(in));
		output[i] = val*master;
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
    if (this->patchGet)
    {
        this->patchGet = false;
        this->updateSampleDelay = 0;
        this->updateMs = 0;
        WriteFrequencyResponse();
    }
	if (this->updateSamples != 0)
	{
		this->updateSamples -= n_samples;
		if (this->updateSamples <= 0 || n_samples == 0)
		{
			this->updateSamples = 0;
			WriteFrequencyResponse();
		}
	}
	if (this->updateMs != 0)
	{
		uint64_t ctime = timeMs();
		if (ctime > this->updateMs || n_samples != 0)
		{
			this->updateMs = 0;
			WriteFrequencyResponse();
		}
	}
	lv2_atom_forge_pop(&forge, &out_frame);
}


float ToobML::CalculateFrequencyResponse(float f)
{
	return toneStackFilter.GetFrequencyResponse(f);
}


LV2_Atom_Forge_Ref ToobML::WriteFrequencyResponse()
{

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
	LV2_Atom_Forge_Ref   set =
		lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch_Set);

    lv2_atom_forge_key(&forge, uris.patch_property);		
	lv2_atom_forge_urid(&forge, uris.param_frequencyResponseVector);
	lv2_atom_forge_key(&forge, uris.patch_value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom_float);
	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		lv2_atom_forge_float(&forge,filterResponse.GetFrequency(i));
		lv2_atom_forge_float(&forge,filterResponse.GetResponse(i));
	}
	lv2_atom_forge_pop(&forge, &vectorFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);
	return set;
}


void ToobML::SetProgram(uint8_t programNumber)
{
	this->programNumber = programNumber;
}


void ToobML::OnMidiCommand(int , int , int )
{
}

void ToobML::OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object)
{
	UNUSED(object);
	if (propertyUrid == uris.param_frequencyResponseVector)
	{
        this->patchGet = true; // 
	}

}
