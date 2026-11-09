/*
 *   Copyright (c) Robin E. R. Davies
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


#include "ToobWarmer.hpp"
#include "LsNumerics/Denorms.hpp"

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

const char* ToobWarmer::URI= TOOB_WARMER_URI;



ToobWarmer::ToobWarmer(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	super(_rate,_bundle_path,features),
	rate(_rate),
	bundle_path(_bundle_path)
{

	uris.Map(this);
	gainSection1.InitUris(this);

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


	this->gainSection1.SetSampleRate(supersampledRate);
	this->masterVolumeDezipped.SetSampleRate(supersampledRate);
	this->sagProcessor.SetSampleRate(supersampledRate);

	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND) + 40;
}

ToobWarmer::~ToobWarmer()
{

}

void ToobWarmer::Activate()
{
    super::Activate();
	
	peakDelay = 0;
	frameTime = 0;
	this->gainSection1.Reset();
	this->sagProcessor.Reset();
	this->masterVolumeDezipped.Reset();
}
void ToobWarmer::Deactivate()
{
}

void ToobWarmer::Run(uint32_t n_samples)
{
    fp_state_t savedFpState = disable_denorms();

	gainSection1.UpdateControls(
        this->trim1,
        this->gain1,
        this->locut1,
        this->hicut1,
        this->bias1,
        this->shape1

    );
	sagProcessor.UpdateControls(
        this->sag,
        this->sagd,
        this->sagf

    );



	if (master.HasChanged())
	{
		this->masterVolumeDezipped.SetTarget(master.GetDb());
	}


	uint32_t ix = 0;
	float lastValue = this->lastvalue;
    const float*inputBuffer = this->in.Get();
    float *outputBuffer = this->out.Get();
	while (ix < n_samples)
	{
		float input = inputBuffer[ix];

		double dx = (input-lastValue)*0.25;

		double lastOutput = 0;
		for (int i = 0; i < 4; ++i)
		{
			lastValue += dx;
			float x = (float)(this->upsamplingFilter.Tick(lastValue));
			//=========
			float x1 = gainSection1.Tick(
						x*sagProcessor.GetInputScale()
							);
			float x4 = sagProcessor.TickOutput(x1);
			float xOut = masterVolumeDezipped.Tick()*x4;

			float absX = std::abs(xOut);
			
			if (absX > this->peakValue)
			{
				this->peakValue = absX;
			}

			//=========

			lastOutput = this->downsamplingFilter.Tick(xOut);
		}
		outputBuffer[ix] = lastOutput;
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
    restore_denorms(savedFpState);

}
LV2_Atom_Forge_Ref ToobWarmer::WriteWaveShape(LV2_URID propertyUrid,GainSection *pGain)
{

	const int NUMBER_OF_POINTS = 101;

	lv2_atom_forge_frame_time(&outputForge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;
	LV2_Atom_Forge_Ref   set =
		lv2_atom_forge_object(&outputForge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&outputForge, uris.patch__property);		
	lv2_atom_forge_urid(&outputForge, propertyUrid);
	lv2_atom_forge_key(&outputForge, uris.patch__value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&outputForge, &vectorFrame, sizeof(float), uris.atom__float);

	
	for (int i = 0; i < NUMBER_OF_POINTS; ++i)
	{
		float x = (float)(i-NUMBER_OF_POINTS) / (NUMBER_OF_POINTS/2);
		float y = pGain->Tick(x);
		lv2_atom_forge_float(&outputForge,y);
	}
	lv2_atom_forge_pop(&outputForge, &vectorFrame);

	lv2_atom_forge_pop(&outputForge, &objectFrame);
	return set;
}



void ToobWarmer::WriteUiState()
{
	lv2_atom_forge_frame_time(&outputForge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;

	lv2_atom_forge_object(&outputForge, &objectFrame, 0, uris.patch__Set);
    lv2_atom_forge_key(&outputForge, uris.patch__property);
	lv2_atom_forge_urid(&outputForge,uris.param_uiState);

    lv2_atom_forge_key(&outputForge, uris.patch__value);		
	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&outputForge, &vectorFrame, sizeof(float), uris.atom__float);


	lv2_atom_forge_float(&outputForge,this->gainSection1.GetPeakMin());
	lv2_atom_forge_float(&outputForge,this->gainSection1.GetPeakMax());
	lv2_atom_forge_float(&outputForge,this->gainSection1.GetPeakOutMin());
	lv2_atom_forge_float(&outputForge,this->gainSection1.GetPeakOutMax());
	lv2_atom_forge_float(&outputForge,sagProcessor.GetSagValue());
	lv2_atom_forge_float(&outputForge,sagProcessor.GetSagDValue());

	this->gainSection1.ResetPeak();

	lv2_atom_forge_pop(&outputForge, &vectorFrame);

	lv2_atom_forge_pop(&outputForge, &objectFrame);

}


void ToobWarmer::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == uris.param_uiState)
	{
		this->WriteUiState();
	}
	if (propertyUrid == uris.waveShapeRequest1)
	{
		gainSection1.WriteShapeCurve(&(this->outputForge), uris.waveShapeRequest1);
	}
}

REGISTRATION_DECLARATION PluginRegistration<ToobWarmer> toobWarmerRegistration(ToobWarmerBase::URI);
