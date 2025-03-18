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

#include <lv2_plugin/Lv2Plugin.hpp>

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "Filters/AudioFilter2.h"
#include "Filters/ShelvingLowCutFilter2.h"
#include "NoiseGate.h"
#include "GainStage.h"
#include "LsNumerics/StagedFft.hpp"



#define SPECTRUM_ANALZER_URI "http://two-play.com/plugins/toob-spectrum"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

namespace toob {
	class SpectrumAnalyzer : public Lv2Plugin {
	private:
		enum class PortId {
			AUDIO_IN = 0,
			AUDIO_OUT,
			CONTROL_IN,
			NOTIFY_OUT,
			MIN_F,
			MAX_F,
			LEVEL
		};
		static constexpr size_t MAX_BUFFER_SIZE = 16*1024;
		static constexpr size_t FFT_SIZE = 16*1024;

		RangedInputPort minF = RangedInputPort(10.0f, 400.0f);
		RangedInputPort maxF = RangedInputPort(1000.0f,22000.0f);
		RangedInputPort level = RangedInputPort(-30,30);

		bool svgPathReady = false;
		const std::string *pSvgPath = nullptr;
		const std::string *pSvgHoldPath = nullptr;


		class FftWorker: public WorkerAction
		{
		private: 
			static constexpr float FRAMES_PER_SECOND = 15;

			enum class FftState {
				Idle,
				Capturing,
				BackgroundProcessing,
				Writing,
				Discarding
			};
			FftState state = FftState::Idle;
			bool enabled = false;
			double sampleRate;
			size_t captureIndex = 0;
			size_t samplesPerUpdate = 0;
			size_t sampleCount = 0;

			SpectrumAnalyzer*pThis;
			size_t blockSize;
			float minFrequency;
			float maxFrequency;
			float dbLevel;
			bool resetHoldValues = true;

			std::vector<float> captureBuffer;


		public:
			FftWorker(SpectrumAnalyzer *pThis)
			:	WorkerAction(pThis),
				pThis(pThis)
			{
			}
			void Initialize(double sampleRate, size_t blockSize, float minFrequency,float maxFrequency,float dbLevel);
			void Reinitialize(float minFrequency, float maxFrequency, float dbLevel);
			void Reset();
			void Deactivate();
			void SetEnabled(bool enabled);
			void OnWriteComplete()
			{
				this->state = FftState::Idle;
			}

			void Tick();

			void Capture(size_t nSamples, const float*values)
			{
				for (size_t i = 0; i < nSamples; ++i)
				{
					captureBuffer[captureIndex++] = values[i];
					if (captureIndex >= captureBuffer.size())
					{
						captureIndex = 0;
					}
				}
				if (sampleCount < this->samplesPerUpdate)
				{
					sampleCount += nSamples;
					if (sampleCount >= this->samplesPerUpdate)
					{
						sampleCount = this->samplesPerUpdate;
					}
					if (sampleCount == this->samplesPerUpdate && state == FftState::Capturing)
					{
						StartBackgroundTask();
						sampleCount = 0;
					}
				}
			}
			size_t GetBlockSize()
			{
				return blockSize;
			}
		protected:
			void OnWork() {
				backgroundTask.CalculateSvgPaths(blockSize,minFrequency,maxFrequency,dbLevel);
			}
			void OnResponse()
			{
				pThis->OnSvgPathReady(this->backgroundTask.svgPath,this->backgroundTask.svgHoldPath);
			}

		private:
			struct BackgroundTask
			{
			private:
				std::vector<float> *pCaptureBuffer;
				size_t capturePosition;
				std::vector<float> fftValues;
				std::vector<float> fftHoldValues;
				std::vector<int64_t> fftHoldTimes;
				std::vector<std::complex<double>> fftResult;
				size_t samplesPerUpdate = 0;

				size_t blockSize = 0;
				double norm =0;
				double sampleRate = 0;
				size_t holdSamples = 0;
				float holdDecay = 0;
				bool resetHoldValues = true;


				float minFrequency = 0;
				float maxFrequency = 0;

				LsNumerics::StagedFft fft {4};
				std::vector<double> fftWindow;

			public:
				std::string svgPath;
				std::string svgHoldPath;
			public:
				void Initialize(FftWorker* fftWorker);
				// convenient way to make sure we don't accidentally share state with audio thread.
				void CaptureData(FftWorker *fftWorker);
				void CopyFromCaptureBuffer();
				void CalculateSvgPaths(size_t blockSize,float minF, float maxF, float dbLevel);
				std::string FftToSvg(const std::vector<float>& fft);
			};

			BackgroundTask backgroundTask;

			void StartBackgroundTask();
			


		};

		FftWorker fftWorker;

		static constexpr  size_t MAX_FFT_SIZE = 8192;

		void OnSvgPathReady(const std::string &svgPath, const std::string&svgHoldPath);
		void WriteSpectrum();

		double sampleRate;
		std::string bundle_path;


		const float* inputL = NULL;
		float* outputL = NULL;

		LV2_Atom_Sequence* controlIn = NULL;
		LV2_Atom_Sequence* notifyOut = NULL;

		LV2_Atom_Forge       forge;        ///< Forge for writing atoms in run thread

		void HandleEvent(LV2_Atom_Event* event);

		struct Urids {
		public:
			void Map(Lv2Plugin* plugin)
			{
				pluginUri = plugin->MapURI(SPECTRUM_ANALZER_URI);

				atom_Path = plugin->MapURI(LV2_ATOM__Path);
				atom__float = plugin->MapURI(LV2_ATOM__Float);
				atom_Int = plugin->MapURI(LV2_ATOM__Int);
				atom_Sequence = plugin->MapURI(LV2_ATOM__Sequence);
				atom__URID = plugin->MapURI(LV2_ATOM__URID);
				atom_eventTransfer = plugin->MapURI(LV2_ATOM__eventTransfer);
				patch__Get = plugin->MapURI(LV2_PATCH__Get);
				patch__Set = plugin->MapURI(LV2_PATCH__Set);
				patch__property = plugin->MapURI(LV2_PATCH__property);
				patch__value = plugin->MapURI(LV2_PATCH__value);
				units__Frame = plugin->MapURI(LV2_UNITS__frame);
				patchProperty__spectrumResponse = plugin->MapURI(TOOB_URI  "#spectrumResponse");
				patchProperty__spectrumEnable = plugin->MapURI(TOOB_URI  "#spectrumEnable");
			}
			LV2_URID patch_accept;

			LV2_URID units__Frame;
			LV2_URID pluginUri;
			LV2_URID atom__float;
			LV2_URID atom_Int;
			LV2_URID atom_Path;
			LV2_URID atom_Sequence;
			LV2_URID atom__URID;
			LV2_URID atom_eventTransfer;
			LV2_URID midi_Event;
			LV2_URID patch__Get;
			LV2_URID patch__Set;
			LV2_URID patch__property;
			LV2_URID patch__value;
			LV2_URID patchProperty__spectrumResponse;
			LV2_URID patchProperty__spectrumEnable;
		};

		Urids urids;


		FilterResponse filterResponse;
		// int32_t peakDelay = 0;
		// float peakValueL = 0;
		// float peakValueR = 0;
	private:
		bool enabled = false;
		int64_t enabledCount = 0;


	protected:
		virtual void OnPatchGet(LV2_URID propertyUrid) override;
		virtual void OnPatchSet(LV2_URID propertyUrid, const LV2_Atom*value) override;

		double getSampleRate() { return sampleRate; }
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
		virtual void ConnectPort(uint32_t port, void* data)  override;
		virtual void Activate()  override;
		virtual void Run(uint32_t n_samples)  override;
		virtual void Deactivate()  override;
	};
}
