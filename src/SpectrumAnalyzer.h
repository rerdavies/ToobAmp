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
// SpectrumAnalyzer.h 

#pragma once

#include "std.h"

#include "lv2/core/lv2.h"
#include "lv2/log/logger.h"
#include "lv2/uri-map/uri-map.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/worker/worker.h"
#include "lv2/patch/patch.h"
#include "lv2/parameters/parameters.h"
#include "lv2/units/units.h"
#include "FilterResponse.h"
#include <string>
#include <complex>

#include "Lv2Plugin.h"

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "Filters/AudioFilter2.h"
#include "Filters/ShelvingLowCutFilter2.h"
#include "CombFilter.h"
#include "NoiseGate.h"
#include "GainStage.h"
#include "LsNumerics/Fft.hpp"



#define SPECTRUM_ANALZER_URI "http://two-play.com/plugins/toob-spectrum"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

namespace TwoPlay {
	class SpectrumAnalyzer : public Lv2Plugin {
	private:
		enum class PortId {
			AUDIO_IN = 0,
			AUDIO_OUT,
			CONTROL_IN,
			NOTIFY_OUT,
			BLOCK_SIZE,
			MIN_F,
			MAX_F
		};

		RangedInputPort blockSize = RangedInputPort(1024.0f, 4096.0f);
		RangedInputPort minF = RangedInputPort(10.0f, 400.0f);
		RangedInputPort maxF = RangedInputPort(1000.0f,22000.0f);

		enum class FftState {
			Idle,
			Capturing,
			BackgroundProcessing,
			Writing
		};
		const std::string *pSvgPath = nullptr;

		FftState fftState = FftState::Idle;

		size_t captureOffset;
		std::vector<float> captureBuffer;


		class FftWorker: public WorkerActionBase
		{
		private: 
			SpectrumAnalyzer*pThis;
			std::string svgPath;
			size_t blockSize;
			float minFrequency;
			float maxFrequency;
		public:
			FftWorker(SpectrumAnalyzer *pThis)
			:	WorkerActionBase(pThis),
				pThis(pThis)
			{
			}

			void SetParameters(size_t blockSize, float minFrequency,float maxFrequency)
			{
				// force block size to power of two.
				size_t t = 1;
				while (t < blockSize)
				{
					t <<= 1;
				}
				if (t < 1024) t = 1024;
				if (t > 8192) t = 8192;
				this->blockSize = t;
				this->minFrequency = minFrequency;
				this->maxFrequency = maxFrequency;
			}
			size_t GetBlockSize()
			{
				return blockSize;
			}
		protected:
			void OnWork() {
				this->svgPath = pThis->GetSvgPath(blockSize,minFrequency,maxFrequency);
			}
			void OnResponse()
			{
				pThis->OnSvgPathReady(this->svgPath);
			}
		};

		FftWorker fftWorker;

		static constexpr  size_t MAX_FFT_SIZE = 8192;

		LsNumerics::Fft fft {4};
		std::vector<std::complex<double> > fftResult;
		std::vector<float> fftWindow;

		std::vector<float> svgBins;

		std::string GetSvgPath(size_t blockSize,float minF, float maxF);
		void RequestSpectrum()
		{
			if (this->fftState == FftState::Idle)
			{
				this->captureOffset = 0;
				this->fftState = FftState::Capturing;
				fftWorker.SetParameters(this->blockSize.GetValue(), this->minF.GetValue(),this->maxF.GetValue());
				this->captureBuffer.resize(fftWorker.GetBlockSize());

			} // else we'll get one real soon anyway.
		}
		void OnSvgPathReady(const std::string &svgPath);
		void WriteSpectrum();

		double rate;
		std::string bundle_path;


		const float* inputL = NULL;
		float* outputL = NULL;

		LV2_Atom_Sequence* controlIn = NULL;
		LV2_Atom_Sequence* notifyOut = NULL;

		LV2_Atom_Forge       forge;        ///< Forge for writing atoms in run thread

		void HandleEvent(LV2_Atom_Event* event);

		struct Uris {
		public:
			void Map(Lv2Plugin* plugin)
			{
				pluginUri = plugin->MapURI(SPECTRUM_ANALZER_URI);

				atom_Path = plugin->MapURI(LV2_ATOM__Path);
				atom_float = plugin->MapURI(LV2_ATOM__Float);
				atom_Int = plugin->MapURI(LV2_ATOM__Int);
				atom_Sequence = plugin->MapURI(LV2_ATOM__Sequence);
				atom_URID = plugin->MapURI(LV2_ATOM__URID);
				atom_eventTransfer = plugin->MapURI(LV2_ATOM__eventTransfer);
				patch_Get = plugin->MapURI(LV2_PATCH__Get);
				patch_Set = plugin->MapURI(LV2_PATCH__Set);
				patch_Put = plugin->MapURI(LV2_PATCH__Put);
				patch_body = plugin->MapURI(LV2_PATCH__body);
				patch_subject = plugin->MapURI(LV2_PATCH__subject);
				patch_property = plugin->MapURI(LV2_PATCH__property);
				patch_accept = plugin->MapURI(LV2_PATCH__accept);
				patch_value = plugin->MapURI(LV2_PATCH__value);
				param_gain = plugin->MapURI(LV2_PARAMETERS__gain);
				unitsFrame = plugin->MapURI(LV2_UNITS__frame);
				param_spectrumResponse = plugin->MapURI(TOOB_URI  "#spectrumResponse");
			}
			LV2_URID patch_accept;

			LV2_URID unitsFrame;
			LV2_URID pluginUri;
			LV2_URID atom_float;
			LV2_URID atom_Int;
			LV2_URID atom_Path;
			LV2_URID atom_Sequence;
			LV2_URID atom_URID;
			LV2_URID atom_eventTransfer;
			LV2_URID midi_Event;
			LV2_URID patch_Get;
			LV2_URID patch_Set;
			LV2_URID patch_Put;
			LV2_URID patch_body;
			LV2_URID patch_subject;
			LV2_URID patch_property;
			LV2_URID patch_value;
			LV2_URID param_gain;
			LV2_URID param_spectrumResponse;
		};

		Uris uris;


		FilterResponse filterResponse;
		int32_t peakDelay = 0;
		float peakValueL = 0;
		float peakValueR = 0;
	private:
		float CalculateFrequencyResponse(float f);

		void SetProgram(uint8_t programNumber);
		LV2_Atom_Forge_Ref WriteFrequencyResponse();
		void WriteSamples(uint32_t count, const float*samples);
	protected:
		virtual void OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object);
		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }

	public:
		static Lv2Plugin* Create(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features)
		{
			return new SpectrumAnalyzer(rate, bundle_path, features);
		}


		SpectrumAnalyzer(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);
		virtual ~SpectrumAnalyzer();

	public:
		static const char* URI;
	protected:
		virtual void ConnectPort(uint32_t port, void* data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();
	};
}
