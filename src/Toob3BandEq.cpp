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

// Toob3BandEq.cpp : Defines the entry point for the application.
//

#include "Toob3BandEq.h"

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

const char* Toob3BandEq::URI_MONO= TOOB_3_BAND_EQU_URI_MONO;
const char* Toob3BandEq::URI_STEREO= TOOB_3_BAND_EQU_URI_STEREO;



uint64_t timeMs();


Toob3BandEq::Toob3BandEq(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(_rate, _bundle_path,features),
	rate(_rate),
	filterResponse(),
	bundle_path(_bundle_path)
{
	uris.Map(this);
	lv2_atom_forge_init(&forge, map);

    this->toneStack.Reset(rate, 2048);

	size_t nChannels = this->stereoConnected ? 2 : 1;
	this->toneStack.PrepareBuffers(nChannels, 2048);

	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND);
	this->updateMsDelay = (1000/MAX_UPDATES_PER_SECOND);
    gainDezipper.SetSampleRate(_rate);
    gainDezipper.SetRate(0.1f); // 100ms dezipper time.
}

Toob3BandEq::~Toob3BandEq()
{

}

void Toob3BandEq::ConnectPort(uint32_t port, void* data)
{
	switch ((PortId)port) {

	case PortId::BASS:
		Bass.SetData(data);
		break;
	case PortId::MID:
		Mid.SetData(data);
		break;
	case PortId::TREBLE:
		Treble.SetData(data);
		break;

    case PortId::GAIN:
        Gain.SetData(data);
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
		case PortId::AUDIO_INR:
		this->inputR = (const float*)data;
		break;
	case PortId::AUDIO_OUTR:
		this->outputR = (float*)data;
		break;
	}
}

void Toob3BandEq::Activate()
{
	
	responseChanged = true;
	frameTime = 0;
	this->stereoConnected = this->inputR != nullptr;
    this->toneStack.Reset(getRate(),2048);
	size_t nChannels = this->stereoConnected ? 2 : 1;
	this->toneStack.PrepareBuffers(nChannels, 2048);
    gainDezipper.Reset(Gain.GetValue());
}
void Toob3BandEq::Deactivate()
{
}

void Toob3BandEq::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t*)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.units__Frame);


	HandleEvents(this->controlIn);

	if (UpdateControls())
	{
		this->responseChanged = true;
	}

    float *inputs[2];
	float *myOutputs[2];


    inputs[0] = const_cast<float*>(this->input);
	inputs[1] = const_cast<float*>(this->inputR);
	myOutputs[0] = this->output;
	myOutputs[1] = this->outputR;
	int numChannels = this->stereoConnected ? 2 : 1;

    float**outputs = toneStack.Process(inputs,numChannels,n_samples);


    for (size_t i = 0; i < n_samples; ++i)
    {
		float zipperValue = gainDezipper.Tick();
		for (int c = 0; c < numChannels; ++c) 
		{
        	myOutputs[c][i] = outputs[c][i]*zipperValue;
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

	lv2_atom_forge_pop(&forge, &out_frame);
}

bool Toob3BandEq::UpdateControls()
{
    if (Gain.HasChanged())
    {
        float gain = Gain.GetValue();
        gainDezipper.SetTarget(gain);
    }
    using Param = BasicNamToneStack::Param;
    bool changed = false;
    if (Bass.HasChanged())
    {
        toneStack.SetParam(Param::Bass, Bass.GetValue());
        changed = true;

    }
    if (Mid.HasChanged())
    {
        toneStack.SetParam(Param::Mid,Mid.GetValue());
        changed = true;
    }
    if (Treble.HasChanged())
    {
        toneStack.SetParam(Param::Treble,Treble.GetValue());
        changed = true;
    }
	return changed;
}

float Toob3BandEq::CalculateFrequencyResponse(float f)
{
    float w = f*M_PI*2/getRate();
    return toneStack.GetFrequencyResponse(w);
}


void Toob3BandEq::WriteFrequencyResponse()
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
	lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&forge, uris.patch__property);		
	lv2_atom_forge_urid(&forge, uris.param_frequencyResponseVector);
	lv2_atom_forge_key(&forge, uris.patch__value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__float);
	
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


void Toob3BandEq::SetProgram(uint8_t programNumber)
{
}


void Toob3BandEq::OnMidiCommand(int cmd0, int cmd1, int cmd2)
{
}

void Toob3BandEq::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == uris.param_frequencyResponseVector)
	{
        this->patchGet = true; // 
	}

}


REGISTRATION_DECLARATION PluginRegistration<Toob3BandEq>
threeBandEqRegistrationMono(TOOB_3_BAND_EQU_URI_MONO);

REGISTRATION_DECLARATION 
PluginRegistration<Toob3BandEq> threeBandEqRegistrationStereo(TOOB_3_BAND_EQU_URI_STEREO);