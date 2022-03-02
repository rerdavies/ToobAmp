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

// ToobTuner.cpp : Defines the entry point for the application.
//

#include "ToobTuner.h"

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
using namespace LsNumerics;

#ifndef _MSC_VER
#include <unistd.h>
#include <signal.h>
#include <csignal>
#endif

using namespace TwoPlay;

static const int MAX_UPDATES_PER_SECOND = 15;


const char* ToobTuner::URI= TOOB_TUNER_URI;



uint64_t timeMs();


ToobTuner::ToobTuner(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(features),
	rate(_rate),
	filterResponse(),
	bundle_path(_bundle_path),
	tunerWorker(this)
{
	uris.Map(this);
	lv2_atom_forge_init(&forge, map);

	subsampleRate = _rate;
	while (subsampleRate > 48000/2)
		subsampleRate /= 2;

	this->tunerWorker.Initialize(subsampleRate);
	this->fftSize = this->tunerWorker.pitchDetector.getFftSize();
	circularBuffer.SetSize(fftSize*3);

	this->lowpassFilter.Design(_rate,0.1,1200,-60,subsampleRate/2);

	this->updateFrameCount = (size_t)(rate/MAX_UPDATES_PER_SECOND);
	this->updateFrameIndex = 0;

}

ToobTuner::~ToobTuner()
{

}

void ToobTuner::ConnectPort(uint32_t port, void* data)
{
	switch ((PortId)port) {

	case PortId::REFFREQ:
		RefFrequency.SetData(data);
		break;
	case PortId::THRESHOLD:
		Threshold.SetData(data);
		break;
	case PortId::MUTE:
		Mute.SetData(data);
		break;
	case PortId::FREQ:
		this->Freq.SetData(data);
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

void ToobTuner::Activate()
{
	requestState = RequestState::Idle;
	frameTime = 0;
	this->lowpassFilter.Reset();
	this->circularBuffer.Reset();


	this->updateFrameIndex = 0;

	this->subsampleIndex = 0;
	this->subsampleCount = (int)(this->rate/this->subsampleRate);

	this->muted = Mute.GetValue() != 0;
	muteDezipper.To(this->muted? 0: 1, 0);
}
void ToobTuner::Deactivate()
{
}

void ToobTuner::Run(uint32_t n_samples)
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

	UpdateControls();

	if (updateFrameCount <= 0 && requestState == RequestState::Idle && circularBuffer.Size() >= this->fftSize)
	{
		requestState = RequestState::Requested;
		this->tunerWorker.Request(circularBuffer.Lock(fftSize));

		// set time (in samples) to next request.
		this->updateFrameIndex = this->updateFrameCount;

	} else {
		--updateFrameCount;
	}
	int subsampleCount = this->subsampleCount;
	int subsampleIndex = this->subsampleIndex;;

	for (uint32_t i = 0; i < n_samples; ++i)
	{
		float v = input[i];
		double subV = lowpassFilter.Tick(v);
		if (++subsampleIndex == subsampleCount)
		{
			subsampleIndex = 0;
			circularBuffer.Add((float)subV);
		}
		output[i] = (float)(v * muteDezipper.Tick());

	}
	this->subsampleCount = subsampleCount;
	this->subsampleIndex = subsampleIndex;
	frameTime += n_samples;

	lv2_atom_forge_pop(&forge, &out_frame);
}

void ToobTuner::UpdateControls()
{
	if (this->RefFrequency.HasChanged())
	{
		this->tunerWorker.pitchDetector.setReferencePitch(this->RefFrequency.GetValue());
	}
	if (this->Threshold.HasChanged())
	{
		this->tunerWorker.thresholdValue = Db2Af(this->Threshold.GetValue());
	}
	if (this->Mute.HasChanged())
	{
		bool muted = this->Mute.GetValue() != 0;
		if (this->muted != muted)
		{
			muteDezipper.To(muted ? 0:1, 0.1f);
		}
	}
}


void ToobTuner::OnPitchReceived(float value) {
	this->Freq.SetValue(value);
	pitchValue = value;
	this->requestState = RequestState::Idle;
	if (this->updateFrameIndex == 0)
	{
		// we underran. Restart the tuner request with a delay.
		this->updateFrameIndex = this->updateFrameCount;
	}
}



void ToobTuner::OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object)
{
	UNUSED(object);
	if (propertyUrid == uris.param_frequencyResponseVector)
	{
	}

}
