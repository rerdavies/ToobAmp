/*
 *   Copyright (c) 2023 Robin E. R. Davies
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
#include "AudioData.hpp"

#include <lv2_plugin/Lv2Plugin.hpp>

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "ControlDezipper.h"
#include "LsNumerics/ConvolutionReverb.hpp"

namespace toob
{

	class ToobConvolutionReverbBase : public Lv2PluginWithState
	{
	private:
		enum class MonoReverbPortId
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
		enum class StereoReverbPortId
		{
			TIME = 0,
			DIRECT_MIX,
			REVERB_MIX,
			WIDTH,
			PAN,
			PREDELAY,
			LOADING_STATE,
			AUDIO_INL,
			AUDIO_INR,
			AUDIO_OUTL,
			AUDIO_OUTR,
			CONTROL_IN,
			CONTROL_OUT
		};

		enum class CabIrPortId
		{
			REVERB_MIX = 0,
			REVERB2_MIX,
			REVERB3_MIX,
			TIME,
			DIRECT_MIX,
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
		enum class PluginType
		{
			ConvolutionReverb,
			ConvolutionReverbStereo,
			CabIr
		};
		static Lv2Plugin *CreateStereoConvolutionReverb(double rate,
														const char *bundle_path,
														const LV2_Feature *const *features)
		{
			return new ToobConvolutionReverbBase(PluginType::ConvolutionReverbStereo, rate, bundle_path, features);
		}

		static Lv2Plugin *CreateCabIR(double rate,
									  const char *bundle_path,
									  const LV2_Feature *const *features)
		{
			return new ToobConvolutionReverbBase(PluginType::CabIr, rate, bundle_path, features);
		}
		ToobConvolutionReverbBase(
			PluginType pluginType,
			double rate,
			const char *bundle_path,
			const LV2_Feature *const *features);
		~ToobConvolutionReverbBase()
		{
		}

	protected:
		static const char *CONVOLUTION_REVERB_URI;
		static const char *CONVOLUTION_REVERB_STEREO_URI;
		static const char *CAB_IR_URI;

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
			ToobConvolutionReverbBase *pThis;

		public:
			using base = WorkerActionWithCleanup;

			LoadWorker(Lv2Plugin *pPlugin);
			void Initialize(size_t sampleRate, ToobConvolutionReverbBase *pReverb);
			bool SetWidth(float width);
			bool SetPan(float pan);
			bool SetTime(float timeInSeconds);
			bool SetFileName(const char *szName);
			bool SetFileName2(const char *szName);
			bool SetFileName3(const char *szName);
			bool SetMix(float mix);
			bool SetMix2(float mix);
			bool SetMix3(float mix);

			bool SetPredelay(bool usePredelay);
			const char *GetFileName() const { return this->fileName; }
			const char *GetFileName2() const { return this->fileName2; }
			const char *GetFileName3() const { return this->fileName3; }
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
			AudioData LoadFile(const std::filesystem::path &fileName, float level);

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

			ToobConvolutionReverbBase *pReverb = nullptr;
			bool changed = false;
			static constexpr size_t MAX_FILENAME = 1024;
			size_t sampleRate = 48000;
			size_t audioBufferSize = 256;
			char fileName[MAX_FILENAME];
			char fileName2[MAX_FILENAME];
			char fileName3[MAX_FILENAME];
			float width = 1;
			float pan = 0;
			float mix = 1;
			float mix2 = 0;
			float mix3 = 0;
			char requestFileName[MAX_FILENAME];
			char requestFileName2[MAX_FILENAME];
			char requestFileName3[MAX_FILENAME];
			float requestMix = 1;
			float requestPan = 0;
			float requestWidth = 1;
			float requestMix2 = 0;
			float requestMix3 = 0;
			convolution_reverb_ptr convolutionReverbResult;
			convolution_reverb_ptr oldConvolutionReverb;
		};

		LoadWorker loadWorker;

		void UpdateConvolution();
		void CancelLoad();

	private:
		bool IsConvolutionReverb() const { return isConvolutionReverb; }

		std::string MapFilename(
			const LV2_Feature *const *features,
			const std::string &input);

		std::string UnmapFilename(const LV2_Feature *const *features, const std::string &fileName);
		void SaveLv2Filename(
			LV2_State_Store_Function store,
			LV2_State_Handle handle,
			const LV2_Feature *const *features,
			LV2_URID urid,
			const std::string &filename);

		PluginType pluginType = PluginType::ConvolutionReverb;
		bool isConvolutionReverb = false;
		void PublishResourceFiles(const LV2_Feature *const *features);

		std::string StringFromAtomPath(const LV2_Atom *pAtom);

		class Urids
		{
		public:
			void Init(Lv2Plugin *plugin)
			{
#define TOOB_Impulse__Prefix "http://two-play.com/plugins/toob-impulse#"
#define TOOB_CABIR__Prefix "http://two-play.com/plugins/toob-cab-ir#"
				reverb__propertyFileName = plugin->MapURI(TOOB_Impulse__Prefix "impulseFile");
				cabir__propertyFileName = plugin->MapURI(TOOB_CABIR__Prefix "impulseFile");
				cabir__propertyFileName2 = plugin->MapURI(TOOB_CABIR__Prefix "impulseFile2");
				cabir__propertyFileName3 = plugin->MapURI(TOOB_CABIR__Prefix "impulseFile3");
				atom__path = plugin->MapURI(LV2_ATOM__Path);
				atom__string = plugin->MapURI(LV2_ATOM__String);
				// convolution__state = plugin->MapURI(TOOB_Impulse__Prefix "state");
			}
			LV2_URID reverb__propertyFileName;
			LV2_URID cabir__propertyFileName;
			LV2_URID cabir__propertyFileName2;
			LV2_URID cabir__propertyFileName3;
			LV2_URID atom__path;
			LV2_URID atom__string;
			// LV2_URID convolution__state;
		};
		Urids urids;

		double getSampleRate() { return sampleRate; }
		double getTime() const { return time; }
		const std::string &getBundlePath() { return bundle_path; }

		std::shared_ptr<LsNumerics::ConvolutionReverb> pConvolutionReverb;

		float time = 2.0f;
		float directMixAf = 0;
		float reverbMixAf = 0;

		std::string bundle_path;

		double sampleRate = 0;
		bool activated = false;
		float *pTime = nullptr;
		float *pDirectMix = nullptr;
		float *pReverbMix = nullptr;
		float *pReverb2Mix = nullptr;
		float *pReverb3Mix = nullptr;
		float *pPredelay = nullptr;
		float *pLoadingState = nullptr;
		float *pWidth = nullptr;
		float *pPan = nullptr;
		const float *inL = nullptr;
		float *outL = nullptr;
		const float *inR = nullptr;
		float *outR = nullptr;
		bool isStereo = false;

		LV2_Atom_Sequence *controlIn = nullptr;
		LV2_Atom_Sequence *controlOut = nullptr;

		float lastTime = -999;
		float lastWidth = 0;
		float lastPan = -999;
		float lastDirectMix = -999;
		float lastReverbMix = -999;
		float lastReverb2Mix = -999;
		float lastReverb3Mix = -999;
		float lastPredelay = -999;
		float lastLoadingState = 0;

		float reverb2MixAf = 0;
		float reverb3MixAf = 0;

		float loadingState = 0.0;

		bool preChangeVolumeZip = false;

		class Loader;
		Loader *pLoader = nullptr;
		void SetDefaultFile(const LV2_Feature *const *features);

		void RequestNotifyOnLoad();
		void NotifyProperties();

		bool stateChanged = false;
		bool notifyReverbFileName = false;
		bool notifyCabIrFileName = false;
		bool notifyCabIrFileName2 = false;
		bool notifyCabIrFileName3 = false;
	};

	class ToobConvolutionReverbMono : public ToobConvolutionReverbBase
	{
	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobConvolutionReverbMono(rate, bundle_path, features);
		}
		static const char *URI;

		ToobConvolutionReverbMono(double rate, const char *bundle_path, const LV2_Feature *const *features)
			: ToobConvolutionReverbBase(PluginType::ConvolutionReverb, rate, bundle_path, features)
		{
		}
	};
	class ToobConvolutionReverbStereo : public ToobConvolutionReverbBase
	{
	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobConvolutionReverbStereo(rate, bundle_path, features);
		}
		static const char *URI;

		ToobConvolutionReverbStereo(double rate, const char *bundle_path, const LV2_Feature *const *features)
			: ToobConvolutionReverbBase(PluginType::ConvolutionReverbStereo, rate, bundle_path, features)
		{
		}
	};
	class ToobConvolutionCabIr : public ToobConvolutionReverbBase
	{
	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobConvolutionCabIr(rate, bundle_path, features);
		}
		static const char *URI;

		ToobConvolutionCabIr(double rate, const char *bundle_path, const LV2_Feature *const *features)
			: ToobConvolutionReverbBase(PluginType::CabIr, rate, bundle_path, features)
		{
		}
	};

} // namespace toob