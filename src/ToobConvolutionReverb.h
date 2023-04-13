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
#include "DbDezipper.h"

#include "Lv2Plugin.h"

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "ControlDezipper.h"
#include "LsNumerics/BalancedConvolution.hpp"

namespace toob
{

	class ToobConvolutionReverb : public Lv2PluginWithState
	{
	private:
		enum class PortId
		{
			TIME = 0,
			DIRECT_MIX,
			REVERB_MIX,
			PREDELAY,
			LOADING_STATE,
			AUDIO_INL,
			AUDIO_OUTL,
			CONTROL_IN,
			CONTROL_OUT
		};
		using convolution_reverb_ptr = std::shared_ptr<ConvolutionReverb>;
		static constexpr const char *VERSION_FILENAME = "ToobAmp.lv2.version";
		static constexpr uint32_t SAMPLE_FILES_VERSION = 1;

	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobConvolutionReverb(rate, bundle_path, features);
		}
		ToobConvolutionReverb(double rate,
							  const char *bundle_path,
							  const LV2_Feature *const *features);
		~ToobConvolutionReverb()
		{
		}

	public:
		static const char *URI;

	protected:
		virtual void ConnectPort(uint32_t port, void *data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();

	protected:
		virtual void OnPatchGet(LV2_URID propertyUrid);
		virtual void OnPatchGetAll();
		virtual void OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *atom);

	private:
		void clear();
		void UpdateControls();
		void MaybeCreateSampleDirectory(const std::filesystem::path &targetDirectory);

		// State extension callbacks.
		virtual LV2_State_Status
		OnRestoreLv2State(
			LV2_State_Retrieve_Function retrieve,
			LV2_State_Handle handle,
			uint32_t flags,
			const LV2_Feature *const *features);

		virtual LV2_State_Status
		OnSaveLv2State(
			LV2_State_Store_Function store,
			LV2_State_Handle handle,
			uint32_t flags,
			const LV2_Feature *const *features);

		void SetLoadingState(float state)
		{
			this->loadingState = state;
			if (this->pLoadingState)
			{
				*(pLoadingState) = state;
			}
		}

		class LoadWorker : public WorkerActionWithCleanup
		{
		private:
			// must aggree with TTL values for State propery.
			enum class State
			{
				NotLoaded = 0,
				Idle = 1,
				Error = 2,
				SentRequest = 3,
				GotResponse = 4,
				CleaningUp = 5,

			};
			ToobConvolutionReverb *pThis;

		public:
			using base = WorkerActionWithCleanup;

			LoadWorker(Lv2Plugin *pPlugin);
			void Initialize(size_t sampleRate, ToobConvolutionReverb *pReverb);
			bool SetTime(float timeInSeconds);
			bool SetFileName(const char *szName);
			bool SetPredelay(bool usePredelay);
			const char *GetFileName() const { return this->fileName; }
			bool Changed() const { return this->changed; }
			bool IsIdle() const { return this->state == State::Idle || this->state == State::Error || this->state == State::NotLoaded; }
			bool IsChanging() const { return this->changed || !IsIdle(); };

			void Tick()
			{ // on audio thread. Don't start loading unless audio is actually running.
				if (IsIdle())
				{
					if (changed)
					{
						changed = false;
						Request();
					}
				}
			}

		private:
			void SetState(State state);
			void Request();
			virtual void OnWork();
			virtual void OnResponse();
			virtual void OnCleanup();
			virtual void OnCleanupComplete();

		private:
			double getRate() { return rate; }
			bool predelay = true;
			bool workingPredelay = true;
			float tailScale = 0;
			float timeInSeconds = -1;
			float workingTimeInSeconds = -1;
			State state = State::NotLoaded;

			bool hasWorkError = false;
			std::string workError;
			double rate = 44100;

			ToobConvolutionReverb *pReverb = nullptr;
			bool changed = false;
			static constexpr size_t MAX_FILENAME = 1024;
			size_t sampleRate = 48000;
			char fileName[MAX_FILENAME];
			char requestFileName[MAX_FILENAME];
			convolution_reverb_ptr convolutionReverbResult;
			convolution_reverb_ptr oldConvolutionReverb;
		};

		LoadWorker loadWorker;

		void UpdateConvolution();
		void CancelLoad();

	private:
		class Urids
		{
		public:
			void Init(Lv2Plugin *plugin)
			{
				propertyFileName = plugin->MapURI("http://two-play.com/impulseFile#impulseFile");
				atom_path = plugin->MapURI(LV2_ATOM__Path);
				convolution__state = plugin->MapURI("http://two-play.com/plugins/toob-convolution-reverb#state");
			}
			LV2_URID propertyFileName;
			LV2_URID atom_path;
			LV2_URID convolution__state;
		};
		Urids urids;

		double getSampleRate() { return sampleRate; }
		double getTime() const { return time; }
		const std::string &getBundlePath() { return bundle_path; }

		std::shared_ptr<LsNumerics::ConvolutionReverb> pConvolutionReverb;

		DbDezipper directMixDezipper;
		DbDezipper reverbMixDezipper;

		float time = 2.0f;
		float directMixDb = -96;
		float reverbMixDb = -96;

		std::string bundle_path;

		double sampleRate = 0;
		bool activated = false;
		float *pTime = nullptr;
		float *pDirectMix = nullptr;
		float *pReverbMix = nullptr;
		float *pPredelay = nullptr;
		float *pLoadingState = nullptr;
		const float *inL = nullptr;
		float *outL = nullptr;
		LV2_Atom_Sequence *controlIn = nullptr;
		LV2_Atom_Sequence *controlOut = nullptr;

		float lastTime = -1;
		float lastDirectMix = -999;
		float lastReverbMix = -999;
		float lastPredelay = -999;
		float lastLoadingState = 0;

		float loadingState = 0.0;

		bool preChangeVolumeZip = false;

		class Loader;
		Loader *pLoader = nullptr;
	};

} // namespace toob