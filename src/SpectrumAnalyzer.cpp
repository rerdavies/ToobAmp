/*
Copyright (c) 2022 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// SpectrumAnalyzer.cpp : Defines the entry point for the application.
//

#include "SpectrumAnalyzer.h"

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
#include <sstream>
#include <iomanip>
#include "LsNumerics/LsMath.hpp"
#include "LsNumerics/Window.hpp"

using namespace std;
using namespace TwoPlay;
using namespace LsNumerics;

#ifndef _MSC_VER
#include <unistd.h>
#include <signal.h>
#include <csignal>
#endif



const char* SpectrumAnalyzer::URI= SPECTRUM_ANALZER_URI;

uint64_t timeMs();

#pragma GCC diagnostic ignored "-Wreorder" 

SpectrumAnalyzer::SpectrumAnalyzer(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(features),
	rate(_rate),
	filterResponse(236),
	bundle_path(_bundle_path),
	fftWorker(this)
{
	uris.Map(this);
	lv2_atom_forge_init(&forge, map);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{

}

void SpectrumAnalyzer::ConnectPort(uint32_t port, void* data)
{
	switch ((PortId)port) {

	case PortId::AUDIO_IN:
		this->inputL = (const float*)data;
		break;
	case PortId::AUDIO_OUT:
		this->outputL = (float*)data;
		break;
	case PortId::CONTROL_IN:
		this->controlIn = (LV2_Atom_Sequence*)data;
		break;
	case PortId::NOTIFY_OUT:
		this->notifyOut = (LV2_Atom_Sequence*)data;
		break;
	case PortId::BLOCK_SIZE:
		this->blockSize.SetData(data);
		break;
	case PortId::MIN_F:
		this->minF.SetData(data);
		break;
	case PortId::MAX_F:
		this->maxF.SetData(data);
		break;

	}
}

void SpectrumAnalyzer::Activate()
{
	LogTrace("SpectrumAnalyzer activated.");
	fftState = FftState::Idle;
	int blockSize = (int)this->blockSize.GetValue();

	captureBuffer.resize(blockSize);
	fft.SetSize(blockSize);


}
void SpectrumAnalyzer::Deactivate()
{
	fftState = FftState::Idle;
	LogTrace("SpectrumAnalyzer deactivated.");
}

void SpectrumAnalyzer::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t*)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.unitsFrame);

	this->HandleEvents(this->controlIn);

	for (uint32_t i = 0; i < n_samples; ++i)
	{
		outputL[i] = inputL[i];
	}

	if (fftState == FftState::Capturing)
	{
		uint32_t samplesThisTime = std::min((uint32_t)(captureBuffer.size()-captureOffset),n_samples);

		for (size_t i = 0; i < samplesThisTime; ++i)
		{
			captureBuffer[captureOffset++] = inputL[i];
		}
		if (captureOffset == captureBuffer.size())
		{
			fftState = FftState::BackgroundProcessing;
			fftWorker.Request();
		}
	}

	if (fftState == FftState::Writing)
	{
		WriteSpectrum();
		fftState = FftState::Idle;
	}
	lv2_atom_forge_pop(&forge, &out_frame);
}


void SpectrumAnalyzer::WriteSpectrum()
{
	lv2_atom_forge_frame_time(&forge, 0);

	LV2_Atom_Forge_Frame objectFrame;

	lv2_atom_forge_object(&forge, &objectFrame, 0, uris.param_spectrumResponse);
	
	lv2_atom_forge_key(&forge, uris.patch_value);
	lv2_atom_forge_string(&forge,this->pSvgPath->c_str(),(uint32_t)(this->pSvgPath->length()));

	lv2_atom_forge_pop(&forge, &objectFrame);

}

void SpectrumAnalyzer::OnSvgPathReady(const std::string &svgPath)
{
	fftState = FftState::Writing;
	this->pSvgPath = &svgPath;
}



void SpectrumAnalyzer::HandleEvent(LV2_Atom_Event* event)
{
	const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&event->body;
	if (lv2_atom_forge_is_object_type(&forge, event->body.type)) {
		if (obj->body.otype == uris.patch_Set) {

			// const LV2_Atom* property = NULL;
			// const LV2_Atom* value = NULL;

			// lv2_atom_object_get(obj,
			// 	uris.patch_property, &property,
			// 	uris.patch_value, &value,
			// 	0);
			// if (!property) {
			// 	LogError("Set message with no property\n");
			// 	return;
			// }
			// else if (property->type != uris.atom_URID) {
			// 	LogError("Set property is not a URID\n");
			// 	return;
			// }
			// uint32_t key = ((const LV2_Atom_URID*)property)->body;
			// if (key == uris.frequencyRequest) {
			// 	const LV2_Atom_URID* accept = NULL;
			// 	const LV2_Atom_Int* n_peaks = NULL;
			// }
		}
		else if (obj->body.otype == uris.patch_Get)
		{
			const LV2_Atom_URID* accept = NULL;
			const LV2_Atom_Float* value = NULL;

			// clang-format off
			lv2_atom_object_get_typed(
				obj,
				uris.patch_accept, &accept, uris.atom_URID,
				uris.param_spectrumResponse, &value, uris.atom_float,
				0);
			if (accept) {
				//RequestSpectrum();
			}
		}
	}
}
void SpectrumAnalyzer::OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object)
{
	UNUSED(object);
	if (propertyUrid == uris.param_spectrumResponse)
	{
		RequestSpectrum();

		// just write a zero response. Results will arrive via an AtomOutput.
		lv2_atom_forge_frame_time(&forge, 0);

		LV2_Atom_Forge_Frame objectFrame;

		lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch_Set);

		lv2_atom_forge_key(&forge, uris.patch_property);		
		lv2_atom_forge_urid(&forge, uris.param_spectrumResponse);
		lv2_atom_forge_key(&forge, uris.patch_value);

		lv2_atom_forge_float(&forge,0.0f);

		lv2_atom_forge_pop(&forge, &objectFrame);
	}
}

const int SPECTRUM_POINTS = 200;

std::string SpectrumAnalyzer::GetSvgPath(size_t blockSize,float minF, float maxF)
{
	std::stringstream s;
	s << std::setprecision(4); 
	fft.SetSize(blockSize);
	fftResult.resize(blockSize);
	if (fftWindow.size() != blockSize)
	{
		fftWindow = LsNumerics::Window::ExactBlackman<float>(blockSize);
	}
	fft.forwardWindowed(this->fftWindow,this->captureBuffer,fftResult);

	float norm = 2/std::sqrt(fftResult.size());
	int lastX = 0;
	int lastValue = 1000;

	float logMinF = std::log(minF);
	float logMaxF = std::log(maxF);


	s << "M0,1000";
	for (size_t i = 0; i < blockSize; ++i) // "= 1": ignore DC since the window function gives is an invalid DC.
	{
		float mag = (float)Af2Db(norm*std::abs(fftResult[i]));
		if (mag < -100) mag = -100;
		float f = i*rate/blockSize;
		float logF = std::log(f);
		int x = (logF-logMinF)*SPECTRUM_POINTS/(logMaxF-logMinF);
		if (x >= SPECTRUM_POINTS) 
		{
			break;
		}
		if (x < 0)
		{
			x = 0;
		}
		float value = 1000*mag/-100;

		if (lastX != x)
		{

			if (x >= 0)
			{
				if (lastX < 0)
				{
					s << " L" << 0 << ',' << lastValue;	
				} else {
					s << " L" << lastX << ',' << lastValue;
				}
			}
			lastX = x;
			lastValue = value;
		} else {
			if (x == lastX && value < lastValue) {
				lastValue = value;
			}
		}
	}
	s << " L" << lastX << "," << lastValue;
	s << " L" << lastX << "," << 1000;
	s << " L" << 0 << "," << 1000; // close the path.
		
	return s.str();

}