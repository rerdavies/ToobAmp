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

// PowerStage.cpp : Defines the entry point for the application.
//

#include "PowerStage.h"

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

const char* PowerStage::URI= "http://two-play.com/plugins/toob-power-stage";



PowerStage::PowerStage(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(features),
	rate(_rate),
	bundle_path(_bundle_path)
{
	LogTrace("PowerStage: Loading");

	uris.Map(this);
	lv2_atom_forge_init(&forge, map);
	LogTrace("PowerStage: Loadedx");
	this->gain1.SetSampleRate(_rate);
	this->gain2.SetSampleRate(_rate);
	this->gain3.SetSampleRate(_rate);
	this->masterVolumeDezipped.SetSampleRate(_rate);
	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND) + 40;
	this->sagProcessor.SetSampleRate(_rate);
}

PowerStage::~PowerStage()
{

}

void PowerStage::ConnectPort(uint32_t port, void* data)
{
	switch ((PortId)port) {

	case PortId::TRIM1:
		this->gain1.Trim.SetData(data);
		break;
	case PortId::GAIN1:
		this->gain1.Gain.SetData(data);
		break;
	case PortId::LOCUT1:
		this->gain1.LoCut.SetData(data);
		break;
	case PortId::HICUT1:
		this->gain1.HiCut.SetData(data);
		break;
	case PortId::SHAPE1:
		this->gain1.Shape.SetData(data);
		break;


	case PortId::GAIN2_ENABLE:
		this->gain2_enable.SetData(data);
		break;
	case PortId::TRIM2:
		this->gain2.Trim.SetData(data);
		break;
	case PortId::GAIN2:
		this->gain2.Gain.SetData(data);
		break;
	case PortId::LOCUT2:
		this->gain2.LoCut.SetData(data);
		break;
	case PortId::HICUT2:
		this->gain2.HiCut.SetData(data);
		break;
	case PortId::SHAPE2:
		this->gain2.Shape.SetData(data);
		break;

	case PortId::GAIN3_ENABLE:
		this->gain3_enable.SetData(data);
		break;
	case PortId::TRIM3:
		this->gain3.Trim.SetData(data);
		break;
	case PortId::GAIN3:
		this->gain3.Gain.SetData(data);
		break;
	case PortId::LOCUT3:
		this->gain3.LoCut.SetData(data);
		break;
	case PortId::HICUT3:
		this->gain3.HiCut.SetData(data);
		break;
	case PortId::SHAPE3:
		this->gain3.Shape.SetData(data);
		break;

	case PortId::SAG:
		this->sagProcessor.Sag.SetData(data);
		break;
	case PortId::SAGD:
		this->sagProcessor.SagD.SetData(data);
		break;
	case PortId::SAGF:
		this->sagProcessor.SagF.SetData(data);
		break;
	case PortId::MASTER:
		this->master.SetData(data);
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

void PowerStage::Activate()
{
	LogTrace("PowerStage activated.");
	
	peakDelay = 0;
	frameTime = 0;
	this->gain1.Reset();
	this->gain2.Reset();
	this->gain3.Reset();
	this->sagProcessor.Reset();
	this->masterVolumeDezipped.Reset();
}
void PowerStage::Deactivate()
{
	LogTrace("PowerStage deactivated.");
}

void PowerStage::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t*)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.unitsFrame);


	this->gain2.Enable = this->gain2_enable.GetValue() > 0.5f;
	this->gain3.Enable = this->gain3_enable.GetValue() > 0.5f;

	gain1.UpdateControls();
	gain2.UpdateControls();
	gain3.UpdateControls();
	sagProcessor.UpdateControls();

	if (master.HasChanged())
	{
		this->masterVolumeDezipped.SetTarget(master.GetDb());
	}

	for (uint32_t i = 0; i < n_samples; ++i)
	{

		float x1 = gain1.Tick(
					input[i]*sagProcessor.GetSagValue()/sagProcessor.GetSagDValue()
						);
		float x2 = gain2.Tick(x1);
		float x3 = gain3.Tick(x2);
		float x4 = sagProcessor.TickOutput(x3);
		float xOut = masterVolumeDezipped.Tick()*x4;

		float absX = std::abs(xOut);
		
		if (absX > this->peakValue)
		{
			this->peakValue = absX;
		}
		output[i] = Undenormalize(xOut);
	}
	frameTime += n_samples;

	this->peakDelay -= n_samples;
	if (this->peakDelay < 0)
	{
		this->peakDelay = this->updateSampleDelay;
		WriteUiState();
		this->peakValue = 0;
	}
	lv2_atom_forge_pop(&forge, &out_frame);
}


void PowerStage::WriteUiState()
{
	lv2_atom_forge_frame_time(&forge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;

	lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch_Set);

    lv2_atom_forge_key(&forge, uris.patch_property);		
	lv2_atom_forge_urid(&forge, uris.param_uiState);
	lv2_atom_forge_key(&forge, uris.patch_value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom_float);


	lv2_atom_forge_float(&forge,this->gain1.GetVu());
	lv2_atom_forge_float(&forge,this->gain2.GetVu());
	lv2_atom_forge_float(&forge,this->gain3.GetVu());
	lv2_atom_forge_float(&forge,this->peakValue);
	lv2_atom_forge_float(&forge,sagProcessor.GetSagValue());
	lv2_atom_forge_float(&forge,sagProcessor.GetSagDValue());


	lv2_atom_forge_pop(&forge, &vectorFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);

}

