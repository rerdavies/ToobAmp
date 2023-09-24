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

// InputStage.cpp : Defines the entry point for the application.
//

#include "InputStage.h"

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

const char *InputStage::URI = "http://two-play.com/plugins/toob-input_stage";

FilterCoefficients2 InputStage::LOWPASS_PROTOTYPE = FilterCoefficients2(
    0.8291449788086549, 0, 0,
    0.8484582463996709, 1.156251050939778, 1);

// Chebyshev HP I, 0.2db ripple, -3db at 1
FilterCoefficients2 InputStage::HIPASS_PROTOTYPE = FilterCoefficients2(
    0, 0, 0.982613364180136,
    1.102510328053848, 1.097734328563927, 1);

#include <time.h>
uint64_t timeMs()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

InputStage::InputStage(double _rate,
                       const char *_bundle_path,
                       const LV2_Feature *const *features)
    : Lv2Plugin(_bundle_path,features)
    ,  rate(_rate)
    ,  bundle_path(_bundle_path)
    ,  programNumber(0)

	, trim(-60.0f, 30.0f)
	, trimOut{-35,10}
	, locut (30.0f,300.0f)
	, bright(0, 25.0f)
	, brightf(1000.0f, 13000.0f)
	, hicut(2000.0f, 13000.0f)
	, gateT(-80.0f, -20.0f)

{
    uris.Map(this);
    lv2_atom_forge_init(&forge, map);
    LogTrace("InputStage: Loadedx");
    this->highCutFilter.SetSampleRate((float)_rate);
    this->loCutFilter.SetSampleRate((float)_rate);
    this->brightFilter.SetSampleRate((float)_rate);
    this->noiseGate.SetSampleRate(_rate);
    this->gainStage.SetSampleRate(_rate);
    this->trimOut.SetSampleRate(_rate);
    this->gateOut.SetSampleRate(_rate);

    this->updateSampleDelay = (int)(_rate / MAX_UPDATES_PER_SECOND);
    this->updateMsDelay = (1000 / MAX_UPDATES_PER_SECOND);
}

InputStage::~InputStage()
{
}

void InputStage::ConnectPort(uint32_t port, void *data)
{
    switch ((PortId)port)
    {

    case PortId::TRIM:
        this->trim.SetData(data);
        break;
    case PortId::TRIM_OUT:
        this->trimOut.SetData(data);
    case PortId::LOCUT:
        this->locut.SetData(data);
        break;
    case PortId::BRIGHT:
        bright.SetData(data);
        break;
    case PortId::BRIGHTF:
        brightf.SetData(data);
        break;
    case PortId::HICUT:
        hicut.SetData(data);
        break;
    case PortId::GATE_T:
        gateT.SetData(data);
        break;
    case PortId::GATE_OUT:
        gateOut.SetData(data);
        break;
    case PortId::AUDIO_IN:
        this->input = (const float *)data;
        break;
    case PortId::AUDIO_OUT:
        this->output = (float *)data;
        break;
    case PortId::CONTROL_IN:
        this->controlIn = (LV2_Atom_Sequence *)data;
        break;
    case PortId::NOTIFY_OUT:
        this->notifyOut = (LV2_Atom_Sequence *)data;
        break;
    }
}

void InputStage::Activate()
{
    LogTrace("InputStage activated.");

    responseChanged = true;
    frameTime = 0;
    trimOut.Reset();
    this->loCutFilter.Reset();
    this->highCutFilter.Reset();
    this->brightFilter.Reset();
    this->noiseGate.Reset();
    this->gainStage.Reset();
    this->gateOut.Reset(0);
}
void InputStage::Deactivate()
{
    LogTrace("InputStage deactivated.");
}

void InputStage::Run(uint32_t n_samples)
{
    // prepare forge to write to notify output port.
    // Set up forge to write directly to notify output port.
    const uint32_t notify_capacity = this->notifyOut->atom.size;
    lv2_atom_forge_set_buffer(
        &(this->forge), (uint8_t *)(this->notifyOut), notify_capacity);

    // Start a sequence in the notify output port.
    LV2_Atom_Forge_Frame out_frame;

    lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.units__Frame);

    this->HandleEvents(this->controlIn);

    float trim = this->trim.GetAf();

    if (this->gateT.HasChanged())
    {
        float db = this->gateT.GetDb();
        noiseGate.SetGateThreshold(db);
        noiseGate.SetEnabled(db != this->gateT.GetMinDb());
    }
    if (this->hicut.HasChanged())
    {
        responseChanged = true;
        float value = hicut.GetValue();
        if (value == hicut.GetMaxValue())
        {
            highCutFilter.Disable();
        }
        else
        {
            highCutFilter.SetCutoffFrequency(value);
        }
    }
    if (this->locut.HasChanged())
    {
        responseChanged = true;
        float value = locut.GetValue();
        if (value == locut.GetMinValue())
        {
            loCutFilter.Disable();
        }
        else
        {
            loCutFilter.SetCutoffFrequency(value);
        }
    }
    if (this->bright.HasChanged())
    {
        brightFilter.SetLowCutDb(this->bright.GetDb());
        responseChanged = true;
    }
    if (this->brightf.HasChanged())
    {
        brightFilter.SetCutoffFrequency(brightf.GetValue());
        responseChanged = true;
    }

    for (uint32_t i = 0; i < n_samples; ++i)
    {

        float x = Undenormalize((float)this->brightFilter.Tick(
            this->highCutFilter.Tick(
                this->loCutFilter.Tick(
                    trim * input[i]))));

        trimOut.AddValue(x);

        float absX = std::abs(x);
        if (absX > this->peakValue)
        {
            this->peakValue = absX;
        }
        x = noiseGate.Tick(x);
        output[i] = x;
    }
    float gateValue;
    switch (noiseGate.GetState())
    {
    case NoiseGate::EState::Attacking:
    case NoiseGate::EState::Holding:
    case NoiseGate::EState::Disabled:
        gateValue = 0.0;
        break;
    default:
        gateValue = 1.0;
        break;
    }
    gateOut.SetValue(gateValue,n_samples);
    frameTime += n_samples;

    trimOut.AddValues(n_samples,output);
    if (responseChanged)
    {
        if (this->patchGet)
        {
            WriteFrequencyResponse();
            this->updateSampleDelay = 0;
            this->updateSamples = 0;
        }
        else
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

    this->peakDelay -= n_samples;
    if (this->peakDelay < 0)
    {
        this->peakDelay = this->updateSampleDelay;
        WriteUiState();
        this->peakValue = 0;
    }
    lv2_atom_forge_pop(&forge, &out_frame);
}

float InputStage::CalculateFrequencyResponse(float f)
{
    return highCutFilter.GetFrequencyResponse(f) * brightFilter.GetFrequencyResponse(f) * loCutFilter.GetFrequencyResponse(f);
}

void InputStage::WriteUiState()
{
    lv2_atom_forge_frame_time(&forge, frameTime);

    LV2_Atom_Forge_Frame objectFrame;

    lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&forge, uris.patch__property);
    lv2_atom_forge_urid(&forge, uris.param_uiState);
    lv2_atom_forge_key(&forge, uris.patch__value);

    LV2_Atom_Forge_Frame vectorFrame;
    lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__float);

    lv2_atom_forge_float(&forge, this->peakValue);
    lv2_atom_forge_float(&forge, (float)(uint)(this->noiseGate.GetState()));

    lv2_atom_forge_pop(&forge, &vectorFrame);

    lv2_atom_forge_pop(&forge, &objectFrame);
}

LV2_Atom_Forge_Ref InputStage::WriteFrequencyResponse()
{

    for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
    {
        filterResponse.SetResponse(
            i,
            this->CalculateFrequencyResponse(
                filterResponse.GetFrequency(i)));
    }

    lv2_atom_forge_frame_time(&forge, frameTime);

    LV2_Atom_Forge_Frame objectFrame;
    LV2_Atom_Forge_Ref set =
        lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&forge, uris.patch__property);
    lv2_atom_forge_urid(&forge, uris.param_frequencyResponseVector);
    lv2_atom_forge_key(&forge, uris.patch__value);

    LV2_Atom_Forge_Frame vectorFrame;
    lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__float);
    for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
    {
        //lv2_atom_forge_float(&forge, filterResponse.GetFrequency(i));
        lv2_atom_forge_float(&forge, filterResponse.GetResponse(i));
    }
    lv2_atom_forge_pop(&forge, &vectorFrame);

    lv2_atom_forge_pop(&forge, &objectFrame);
    return set;
}

void InputStage::SetProgram(uint8_t programNumber)
{
    this->programNumber = programNumber;
}

void InputStage::OnMidiCommand(int cmd0, int cmd1, int cmd2)
{
    UNUSED(cmd2);
    if (cmd0 == LV2_MIDI_MSG_PGM_CHANGE)
    {
        SetProgram(cmd1);
    }
}

void InputStage::HandleEvent(LV2_Atom_Event *event)
{
    const LV2_Atom_Object *obj = (const LV2_Atom_Object *)&event->body;
    if (lv2_atom_forge_is_object_type(&forge, event->body.type))
    {
        if (obj->body.otype == uris.patch__Set)
        {

            // const LV2_Atom* property = NULL;R
            // const LV2_Atom* value = NULL;

            // lv2_atom_object_get(obj,
            // 	uris.patch__property, &property,
            // 	uris.patch__value, &value,
            // 	0);
            // if (!property) {
            // 	LogError("Set message with no property\n");
            // 	return;
            // }
            // else if (property->type != uris.atom__URID) {
            // 	LogError("Set property is not a URID\n");
            // 	return;
            // }
            // uint32_t key = ((const LV2_Atom_URID*)property)->body;
            // if (key == uris.frequencyRequest) {
            // 	const LV2_Atom_URID* accept = NULL;
            // 	const LV2_Atom_Int* n_peaks = NULL;
            // }
        }
        else if (obj->body.otype == uris.patch__Get)
        {
            const LV2_Atom_URID *accept = NULL;
            const LV2_Atom_Float *value = NULL;

            // clang-format off
			lv2_atom_object_get_typed(
				obj,
				uris.patch_accept, &accept, uris.atom__URID,
				uris.frequencyRequest, &value, uris.atom__float,
				0);
			if (accept && accept->body == uris.frequencyRequest) {
				// Received a request for peaks, prepare for transmission
				WriteFrequencyResponse();
			}
		}
	}
}

void InputStage::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == uris.param_frequencyResponseVector)
	{
		this->responseChanged = true; // start a potentially delayed update
        this->patchGet = true; // but don't delay it.
	}

}
