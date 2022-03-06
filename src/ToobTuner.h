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
// ToobTuner.h 

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
#include "LsNumerics/PitchDetector.hpp"
#include <string>
#include "Filters/ChebyshevDownsamplingFilter.h"
#include "ControlDezipper.h"
#include "CircularBuffer.h"

#include "Lv2Plugin.h"

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"



#define TOOB_TUNER_URI "http://two-play.com/plugins/toob-tuner"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif


namespace TwoPlay {
	using namespace LsNumerics;
	
	class ToobTuner : public Lv2Plugin {
	private:
		enum class PortId {
			REFFREQ = 0,
			THRESHOLD,
			MUTE,
			FREQ,
			AUDIO_IN,
			AUDIO_OUT,
			CONTROL_IN,
			NOTIFY_OUT,
		};

		double rate;
		std::string bundle_path;

		const float* input = NULL;;
		float* output = NULL;;

		LV2_Atom_Sequence* controlIn = NULL;
		LV2_Atom_Sequence* notifyOut = NULL;
		uint64_t frameTime = 0;

		ChebyshevDownsamplingFilter lowpassFilter;
		double subsampleRate;
		size_t fftSize;
		int subsampleCount;
		int subsampleIndex;
		int updateFrameCount;
		int updateFrameIndex;

		enum class RequestState {
			Idle,
			Requested
		};

		RequestState requestState = RequestState::Idle;

		CircularBuffer<float> circularBuffer;

		LV2_Atom_Forge       forge;        ///< Forge for writing atoms in run thread

		struct Uris {
		public:
			void Map(Lv2Plugin* plugin)
			{
				pluginUri = plugin->MapURI(TOOB_TUNER_URI);

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
			}
			LV2_URID patch_accept;

			LV2_URID frequencyResponseUri;
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
			LV2_URID param_frequencyResponseVector;
		};

		Uris uris;


		FilterResponse filterResponse;

		class TunerWorker: public WorkerActionBase
		{
		private: 
			ToobTuner*pThis;
			CircularBuffer<float>::LockResult lockResult;
			float pitchResult;
		public:

			PitchDetector pitchDetector;
			float thresholdValue = 0;

			TunerWorker(ToobTuner *pThis)
			:	WorkerActionBase(pThis),
				pThis(pThis)
			{
			}
			void Initialize(double subSampleRate)
			{
				pitchDetector.Initialize((int)subSampleRate);
			}

			void Request(const CircularBuffer<float>::LockResult & lockResult)
			{
				this->lockResult = lockResult;
				this->WorkerActionBase::Request();
			}	
		protected:
			void OnWork() {
				bool aboveThreshold = false;
				for (auto i = this->lockResult.begin(); i != this->lockResult.end(); ++i)
				{
					if (*i > this->thresholdValue)
					{
						aboveThreshold = true;
						break;
					}
				}
				if (aboveThreshold)
				{
					pitchResult = pitchDetector.detectPitch(this->lockResult.begin(),this->lockResult.end());
				} else {
					pitchResult = 0;
				}
			}
			void OnResponse()
			{
				pThis->OnPitchReceived(this->pitchResult);
			}
		};

		TunerWorker tunerWorker;


	protected:
		virtual void OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object);

	private:
        RangedInputPort RefFrequency =RangedInputPort(425,455);
		RangedInputPort Threshold =RangedInputPort(-60,0);
		RangedInputPort Mute =RangedInputPort(0,1);
		OutputPort Freq {0};
		double pitchValue = -1;

		void OnPitchReceived(float value);

		bool muted = false;
		ControlDezipper muteDezipper {0};

		void UpdateControls();

	protected:
		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }

	public:
		static Lv2Plugin* Create(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features)
		{
			return new ToobTuner(rate, bundle_path, features);
		}



		ToobTuner(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);
		virtual ~ToobTuner();

	public:
		static const char* URI;
	protected:
		virtual void ConnectPort(uint32_t port, void* data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();
	};
};
