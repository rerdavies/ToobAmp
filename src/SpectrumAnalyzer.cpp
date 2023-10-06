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
using namespace toob;
using namespace LsNumerics;

#ifndef _MSC_VER
#include <unistd.h>
#include <signal.h>
#include <csignal>
#endif




constexpr size_t MAX_BLOCKSIZE = 32*1024;
const char *SpectrumAnalyzer::URI = SPECTRUM_ANALZER_URI;

uint64_t timeMs();

#pragma GCC diagnostic ignored "-Wreorder"

SpectrumAnalyzer::SpectrumAnalyzer(double _rate,
								   const char *_bundle_path,
								   const LV2_Feature *const *features)
	: Lv2Plugin(_bundle_path,features),
	  sampleRate(_rate),
	  filterResponse(236),
	  bundle_path(_bundle_path),
	  fftWorker(this)
{
	urids.Map(this);
	lv2_atom_forge_init(&forge, map);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
}

void SpectrumAnalyzer::ConnectPort(uint32_t port, void *data)
{
	switch ((PortId)port)
	{

	case PortId::AUDIO_IN:
		this->inputL = (const float *)data;
		break;
	case PortId::AUDIO_OUT:
		this->outputL = (float *)data;
		break;
	case PortId::CONTROL_IN:
		this->controlIn = (LV2_Atom_Sequence *)data;
		break;
	case PortId::NOTIFY_OUT:
		this->notifyOut = (LV2_Atom_Sequence *)data;
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
	fftWorker.Initialize(getSampleRate(),FFT_SIZE,minF.GetValue(),maxF.GetValue());
}
void SpectrumAnalyzer::Deactivate()
{
	fftWorker.Deactivate();
}

void SpectrumAnalyzer::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t *)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, urids.units__Frame);

	this->HandleEvents(this->controlIn);

	if (this->minF.HasChanged() || this->maxF.HasChanged())
	{
		fftWorker.Reinitialize(minF.GetValue(),maxF.GetValue());
	}

	fftWorker.Tick();


	for (uint32_t i = 0; i < n_samples; ++i)
	{
		outputL[i] = inputL[i];
	}

	fftWorker.Capture(n_samples,inputL);


	if (this->svgPathReady)
	{
		this->svgPathReady = false;
		WriteSpectrum();
		fftWorker.OnWriteComplete();
	}


	lv2_atom_forge_pop(&forge, &out_frame);
}

void SpectrumAnalyzer::WriteSpectrum()
{
	if (!enabled)
		return;
	lv2_atom_forge_frame_time(&forge, 0);

	LV2_Atom_Forge_Frame objectFrame;

	lv2_atom_forge_object(&forge, &objectFrame, 0, urids.patch__Set);
	lv2_atom_forge_key(&forge, urids.patch__property);
	lv2_atom_forge_urid(&forge, urids.patchProperty__spectrumResponse);

	lv2_atom_forge_key(&forge, urids.patch__value);
	LV2_Atom_Forge_Frame tupleFrame;
	lv2_atom_forge_tuple(&forge,&tupleFrame);
	{
		lv2_atom_forge_string(&forge, this->pSvgPath->c_str(), (uint32_t)(this->pSvgPath->length()));
		lv2_atom_forge_string(&forge, this->pSvgHoldPath->c_str(), (uint32_t)(this->pSvgHoldPath->length()));
	}
	lv2_atom_forge_pop(&forge,&tupleFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);
}

void SpectrumAnalyzer::OnSvgPathReady(const std::string &svgPath,const std::string&svgHoldPath)
{
	this->svgPathReady = true;
	this->pSvgPath = &svgPath;
	this->pSvgHoldPath = &svgHoldPath;
}

void SpectrumAnalyzer::HandleEvent(LV2_Atom_Event *event)
{
	const LV2_Atom_Object *obj = (const LV2_Atom_Object *)&event->body;
	if (lv2_atom_forge_is_object_type(&forge, event->body.type))
	{
		if (obj->body.otype == urids.patch__Set)
		{

			// const LV2_Atom* property = NULL;
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
		else if (obj->body.otype == urids.patch__Get)
		{
			const LV2_Atom_URID *accept = NULL;
			const LV2_Atom_Float *value = NULL;

			// clang-format off
			lv2_atom_object_get_typed(
				obj,
				urids.patch_accept, &accept, urids.atom__URID,
				urids.patchProperty__spectrumResponse, &value, urids.atom__float,
				0);
			if (accept) {
				//RequestSpectrum();
			}
		}
	}
}


void SpectrumAnalyzer::OnPatchSet(LV2_URID propertyUrid, const LV2_Atom*value) 
{
	if (propertyUrid == urids.patchProperty__spectrumEnable)
	{
		LV2_Atom_Bool *pVal = (LV2_Atom_Bool*)value;
		bool enabled = pVal->body != 0;
		if (enabled != this->enabled)
		{
			this->enabled = enabled;
			fftWorker.SetEnabled(enabled);
		}
	}
}



void SpectrumAnalyzer::OnPatchGet(LV2_URID propertyUrid)
{
}

const int SPECTRUM_POINTS = 200;

void SpectrumAnalyzer::FftWorker::Reset()
{
	resetHoldValues = true;
	sampleCount = 0;
}

void SpectrumAnalyzer::FftWorker::Reinitialize(float minFrequency,float maxFrequency)
{
	// force block size to power of two.
	this->minFrequency = minFrequency;
	this->maxFrequency = maxFrequency;

	if (state == FftState::Capturing)
	{
		state = FftState::Idle;
	} else if (state != FftState::Idle)
	{
		state = FftState::Discarding; // any pending results will be discarded.
	}

	Reset();
	sampleCount = 0;
}

void SpectrumAnalyzer::FftWorker::SetEnabled(bool enabled)
{
	if (this->enabled != enabled)
	{
		this->enabled = enabled;
		if (!enabled)
		{
			if (state == FftState::Capturing)
			{
				state = FftState::Idle;
			} else if (state != FftState::Idle)
			{
				state = FftState::Discarding; // any pending results will be discarded.
			}
		}
	}
}

void SpectrumAnalyzer::FftWorker::BackgroundTask::Initialize(FftWorker*fftWorker)
{

	this->samplesPerUpdate = fftWorker->samplesPerUpdate;
	blockSize = fftWorker->blockSize;
	this->sampleRate = fftWorker->sampleRate;

	fft.SetSize(blockSize);
	fftResult.resize(blockSize);


	this->norm = 2/std::sqrt(blockSize);


	fftValues.resize(blockSize/2);
	fftHoldValues.resize(blockSize/2);
	fftHoldTimes.resize(0);
	fftHoldTimes.resize(blockSize/2);

	constexpr float HOLD_TIME_SECONDS = 2.0;
	this->holdSamples = (size_t)(sampleRate*HOLD_TIME_SECONDS);
	if (holdSamples < blockSize) holdSamples = blockSize;
	constexpr float DECAY_TIME = 2.0;

	this->holdDecay = -60*(samplesPerUpdate/(DECAY_TIME*sampleRate));

	fftWindow = LsNumerics::Window::FlatTop<double>(blockSize);
}
void SpectrumAnalyzer::FftWorker::Initialize(double sampleRate, size_t blockSize, float minFrequency,float maxFrequency)
{

	captureBuffer.resize(MAX_BUFFER_SIZE + sampleRate*0.5);

	this->sampleRate = sampleRate;
	this->minFrequency = minFrequency;
	this->maxFrequency = maxFrequency;
	size_t t = 1;
	while (t < blockSize)
	{
		t <<= 1;
	}
	if (t < 1024) t = 1024;
	if (t > MAX_BLOCKSIZE) t = MAX_BLOCKSIZE;

	this->blockSize = blockSize;

	this->samplesPerUpdate = (size_t)(sampleRate/FRAMES_PER_SECOND);
	backgroundTask.Initialize(this);
	Reset();
}


void SpectrumAnalyzer::FftWorker::StartBackgroundTask()
{
	if (this->state == FftState::Capturing)
	{
		this->state = FftState::BackgroundProcessing;
		backgroundTask.CaptureData(this);
		this->Request();
		this->sampleCount = 0;
	}
}
void SpectrumAnalyzer::FftWorker::BackgroundTask::CaptureData(FftWorker *fftWorker)
{
	// just what's required to ensure that the background task runs with data that won't be modified by the foreground task.
	this->resetHoldValues = fftWorker->resetHoldValues;
	fftWorker->resetHoldValues = false;
	this->minFrequency = fftWorker->minFrequency;
	this->maxFrequency = fftWorker->maxFrequency;

	this->capturePosition = fftWorker->captureIndex;
	this->pCaptureBuffer = &(fftWorker->captureBuffer);
}

void SpectrumAnalyzer::FftWorker::BackgroundTask::CopyFromCaptureBuffer()
{
	size_t captureStart = this->capturePosition;
	if (capturePosition < blockSize)
	{
		captureStart = pCaptureBuffer->size()-(blockSize-capturePosition);
	} else {
		captureStart = capturePosition - blockSize;
	}
	size_t captureEnd = captureStart+blockSize;

	if (captureEnd <= pCaptureBuffer->size())
	{
		// we can do it in one go.
		size_t ix = 0;
		for (size_t i = captureStart; i < captureEnd;++i)
		{
			float value = (*pCaptureBuffer)[i];
			fftResult[ix++] = std::complex<double>(value,0);
		}
	} else {
		// data wraps around. Two segments.
		captureEnd -= pCaptureBuffer->size();

		size_t ix = 0;
		for (size_t i = captureStart; i < pCaptureBuffer->size(); ++i)
		{
			float value = (*pCaptureBuffer)[i];
			fftResult[ix++] = std::complex<double>(value,0);
		}
		for (size_t i = 0; i < captureEnd;++i)
		{
			float value = (*pCaptureBuffer)[i];
			fftResult[ix++] = std::complex<double>(value,0);
		}
	}
}
void SpectrumAnalyzer::FftWorker::BackgroundTask::CalculateSvgPaths(size_t blockSize,float minF, float maxF)
{
	if (this->resetHoldValues)
	{
		this->resetHoldValues = false;

		for (size_t i = 0; i < fftHoldValues.size(); ++i)
		{
			fftHoldValues[i] = -200;
		}
	}

	// copy fft data out of the capture buffer.
	CopyFromCaptureBuffer();


	fft.Forward(fftResult,fftResult);


	for (size_t i = 0; i < fftValues.size(); ++i)
	{
		fftValues[i] = Af2Db(norm*std::abs(fftResult[i]));
	}

	for (size_t i = 0; i < fftValues.size(); ++i)
	{
		float x = fftHoldValues[i];
		int64_t t = fftHoldTimes[i];
		t -= samplesPerUpdate;
		if (t <= 0)
		{
			t = 0;
			x += this->holdDecay;
			if (x < -200)
			{
				x = -200;
			}
		} 
		float result = fftValues[i];
		if (result > x)
		{
			x = result;
			t = this->holdSamples;
		}
		fftHoldValues[i] = x;
		fftHoldTimes[i] = t;
	}

	this->svgPath = FftToSvg(fftValues);
	this->svgHoldPath = FftToSvg(fftHoldValues);
}
std::string SpectrumAnalyzer::FftWorker::BackgroundTask::FftToSvg(const std::vector<float>& fft)
{
	int lastX = 0;
	int lastValue = 1000;

	float logMinF = std::log(minFrequency);
	float logMaxF = std::log(maxFrequency);

	std::stringstream s;
	s << std::setprecision(4); 
	s << "M0,1000";
	for (size_t i = 1; i < fft.size(); ++i) // "= 1": ignore DC since the window function gives is a DC that fluctuates wildly.
	{
		float mag = fft[i];
		if (mag < -150) mag = -150;
		float f = i*sampleRate/blockSize;
		float logF = std::log(f);
		int x = (logF-logMinF)*SPECTRUM_POINTS/(logMaxF-logMinF);
		constexpr float MAX_DB = 0;
		constexpr float MIN_DB = -100;
		constexpr float MAX_Y = 0;
		constexpr float MIN_Y = 1000;
		constexpr float SCALE = (MAX_Y-MIN_Y)/(MAX_DB-MIN_DB);

		float value = (mag-MIN_DB)*SCALE+MIN_Y;

		if (lastX != x)
		{

			if (x >= 0)
			{
				if (x >= SPECTRUM_POINTS) 
				{
					float blend = (SPECTRUM_POINTS-lastX)/(x-lastX);
					float xN = lastValue*(1-blend)+value*blend;
					s << " L" << SPECTRUM_POINTS << ',' << xN;	
					break;
				}
				else if (lastX < 0 && x != 0)
				{
					float blend = -lastX/(x-lastX);
					float x0 = lastValue*(1-blend)+value*blend;
				
					s << " L" << 0 << ',' << x0;	
					s << " L" << x << ',' << value;	
				} else {
					s << " L" << x << ',' << value;
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





void SpectrumAnalyzer::FftWorker::Tick()
{
	if (state == FftState::Idle && enabled)
	{
		state = FftState::Capturing;
		if (this->sampleCount >= this->samplesPerUpdate)
		{
			this->StartBackgroundTask();
		}
	}
	
}

void   SpectrumAnalyzer::FftWorker::Deactivate()
{
	this->state = FftState::Discarding;
}





		