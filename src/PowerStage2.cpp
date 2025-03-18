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


#include "PowerStage2.h"

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

#ifndef _MSC_VER
#include <unistd.h>
#include <signal.h>
#include <csignal>
#endif


const int MAX_UPDATES_PER_SECOND = 10;
//const int UPSAMPLING_BUFFER_SIZE = 128;

const char* PowerStage2::URI= POWER_STAGE_2_URI;



PowerStage2::PowerStage2(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(_rate,_bundle_path,features),
	rate(_rate),
	bundle_path(_bundle_path)
{
	LogTrace("PowerStage2: Loading");

	uris.Map(this);
	gain1.InitUris(this);
	gain2.InitUris(this);
	gain3.InitUris(this);
	lv2_atom_forge_init(&forge, map);
	LogTrace("PowerStage2: Loadedx");

	double downsamplingCutoff = 18000;
	if (rate < 48000)
	{
		downsamplingCutoff = rate*18000/48000;
	}
	double downsamplingBandStop = (rate-downsamplingCutoff);
	const double BANDSTOP_DB = -80;

	double supersampledRate = _rate*4;

	upsamplingFilter.Design(supersampledRate,0.5,downsamplingCutoff,BANDSTOP_DB,downsamplingBandStop);
	downsamplingFilter.Design(supersampledRate,0.5,downsamplingCutoff,BANDSTOP_DB,downsamplingBandStop);


	this->gain1.SetSampleRate(supersampledRate);
	this->gain2.SetSampleRate(supersampledRate);
	this->gain3.SetSampleRate(supersampledRate);
	this->masterVolumeDezipped.SetSampleRate(supersampledRate);
	this->sagProcessor.SetSampleRate(supersampledRate);

	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND) + 40;
}

PowerStage2::~PowerStage2()
{

}

void PowerStage2::ConnectPort(uint32_t port, void* data)
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
	case PortId::BIAS1:
		this->gain1.Bias.SetData(data);
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
	case PortId::BIAS2:
		this->gain2.Bias.SetData(data);
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
	case PortId::BIAS3:
		this->gain3.Bias.SetData(data);
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

void PowerStage2::Activate()
{
	LogTrace("PowerStage2 activated.");
	
	peakDelay = 0;
	frameTime = 0;
	this->gain1.Reset();
	this->gain2.Reset();
	this->gain3.Reset();
	this->sagProcessor.Reset();
	this->masterVolumeDezipped.Reset();
}
void PowerStage2::Deactivate()
{
	LogTrace("PowerStage2 deactivated.");
}

void PowerStage2::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t*)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.units__Frame);

	this->gain2.Enable = this->gain2_enable.GetValue() > 0.5f;
	this->gain3.Enable = this->gain3_enable.GetValue() > 0.5f;

	gain1.UpdateControls();
	gain2.UpdateControls();
	gain3.UpdateControls();
	sagProcessor.UpdateControls();

    this->HandleEvents(this->controlIn);


	if (master.HasChanged())
	{
		this->masterVolumeDezipped.SetTarget(master.GetDb());
	}


	uint32_t ix = 0;
	float lastValue = this->lastvalue;
	while (ix < n_samples)
	{
		float input = this->input[ix];

		double dx = (input-lastValue)*0.25;

		double lastOutput = 0;
		for (int i = 0; i < 4; ++i)
		{
			lastValue += dx;
			float x = (float)(this->upsamplingFilter.Tick(lastValue));
			//=========
			float x1 = gain1.Tick(
						x*sagProcessor.GetInputScale()
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

			//=========

			lastOutput = this->downsamplingFilter.Tick(xOut);
		}
		this->output[ix] = Undenormalize(lastOutput);
		lastValue = input;
		++ix;
	}	
	this->lastvalue = lastValue;
	
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
LV2_Atom_Forge_Ref PowerStage2::WriteWaveShape(LV2_URID propertyUrid,GainSection *pGain)
{

	const int NUMBER_OF_POINTS = 101;

	lv2_atom_forge_frame_time(&forge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;
	LV2_Atom_Forge_Ref   set =
		lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&forge, uris.patch__property);		
	lv2_atom_forge_urid(&forge, propertyUrid);
	lv2_atom_forge_key(&forge, uris.patch__value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__float);

	
	for (int i = 0; i < NUMBER_OF_POINTS; ++i)
	{
		float x = (float)(i-NUMBER_OF_POINTS) / (NUMBER_OF_POINTS/2);
		float y = pGain->Tick(x);
		lv2_atom_forge_float(&forge,y);
	}
	lv2_atom_forge_pop(&forge, &vectorFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);
	return set;
}



void PowerStage2::WriteUiState()
{
	lv2_atom_forge_frame_time(&forge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;

	lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);
    lv2_atom_forge_key(&forge, uris.patch__property);
	lv2_atom_forge_urid(&forge,uris.param_uiState);

    lv2_atom_forge_key(&forge, uris.patch__value);		
	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__float);


	lv2_atom_forge_float(&forge,this->gain1.GetPeakMin());
	lv2_atom_forge_float(&forge,this->gain1.GetPeakMax());
	lv2_atom_forge_float(&forge,this->gain1.GetPeakOutMin());
	lv2_atom_forge_float(&forge,this->gain1.GetPeakOutMax());
	lv2_atom_forge_float(&forge,this->gain2.GetPeakMin());
	lv2_atom_forge_float(&forge,this->gain2.GetPeakMax());
	lv2_atom_forge_float(&forge,this->gain2.GetPeakOutMin());
	lv2_atom_forge_float(&forge,this->gain2.GetPeakOutMax());
	lv2_atom_forge_float(&forge,this->gain3.GetPeakMin());
	lv2_atom_forge_float(&forge,this->gain3.GetPeakMax());
	lv2_atom_forge_float(&forge,this->gain3.GetPeakOutMin());
	lv2_atom_forge_float(&forge,this->gain3.GetPeakOutMax());
	lv2_atom_forge_float(&forge,sagProcessor.GetSagValue());
	lv2_atom_forge_float(&forge,sagProcessor.GetSagDValue());

	this->gain1.ResetPeak();
	this->gain2.ResetPeak();
	this->gain3.ResetPeak();

	lv2_atom_forge_pop(&forge, &vectorFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);

}


void PowerStage2::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == uris.param_uiState)
	{
		this->WriteUiState();
	}
	if (propertyUrid == uris.waveShapeRequest1)
	{
		gain1.WriteShapeCurve(&(this->forge), uris.waveShapeRequest1);
	}
	if (propertyUrid == uris.waveShapeRequest2)
	{
		gain2.WriteShapeCurve(&this->forge, uris.waveShapeRequest2);
	}
	if (propertyUrid == uris.waveShapeRequest3)
	{
		gain3.WriteShapeCurve(&this->forge, uris.waveShapeRequest3);
	}
}

#ifdef JUNK
void PowerStage2::HandleEvent(LV2_Atom_Event* event)
{
	const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&event->body;
	if (lv2_atom_forge_is_object_type(&forge, event->body.type)) {
		if (obj->body.otype == uris.patch__Set) {
		}
		else if (obj->body.otype == uris.patch__Get)
		{
			const LV2_Atom_URID* accept = NULL;
			const LV2_Atom_Float* value = NULL;

			// clang-format off
			lv2_atom_object_get_typed(
				obj,
				uris.patch_accept, &accept, uris.atom__URID,
				uris.waveShapeRequest1, &value, uris.atom__float,
				0);
			if (accept) {
				if (accept->body == uris.waveShapeRequest1) {
					WriteWaveShape(uris.waveShapeRequest1,&(this->gain1));
				} else if (accept->body == uris.waveShapeRequest2) {
					WriteWaveShape(uris.waveShapeRequest2,&(this->gain2));
				} else if (accept->body == uris.waveShapeRequest2) {
					WriteWaveShape(uris.waveShapeRequest3,&(this->gain3));
				}

			}
		}
	}

}
#endif
