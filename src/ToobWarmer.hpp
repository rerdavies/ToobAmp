/*
 *   Copyright (c) Robin E.R. Davies
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

// InputStage.h 

#pragma once

#include "std.h"


#include "InputPort.h"
// Disambiguate conflict between legacy InputPort.h definitions and lv2 inpuit port definitions.
namespace warmer_plugin {
    using RangedDbInputPort = toob::RangedDbInputPort;
    using RangedInputPort = toob::RangedInputPort;
    using SteppedInputPort = toob::SteppedInputPort;
}


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

#include <lv2_plugin/Lv2Plugin.hpp>

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "Filters/AudioFilter2.h"
#include "Filters/ShelvingLowCutFilter2.h"
#include "NoiseGate.h"
#include "GainStage.h"
#include "GainSection.h"
#include "DbDezipper.h"
#include "SagProcessor.h"
#include "Filters/ChebyshevDownsamplingFilter.h"



#define TOOB_WARMER_URI "http://two-play.com/plugins/toob-warmer"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif
#define WAVESHAPE_REQUEST_URI TOOB_URI "#waveShape"


#define DEFINE_LV2_PLUGIN_BASE
#include "ToobWarmerInfo.hpp"

using namespace warmer_plugin;

namespace toob {
	class ToobWarmer : public ToobWarmerBase {
        using super = ToobWarmerBase;
	private:

		double rate;
		std::string bundle_path;

		GainSection gainSection1;
		SagProcessor sagProcessor;


		DbDezipper masterVolumeDezipped;

		ChebyshevDownsamplingFilter upsamplingFilter;
		ChebyshevDownsamplingFilter downsamplingFilter;
		float *upsampledInputBuffer = nullptr;
		float *upsampledOutputBuffer = nullptr;
		float lastvalue = 0;


		uint64_t frameTime = 0;

		int64_t updateSampleDelay;
		int64_t updateSamples = 0;



		struct Uris {
		public:
			void Map(Lv2Plugin* plugin)
			{
				pluginUri = plugin->MapURI(TOOB_WARMER_URI);

				atom_Path = plugin->MapURI(LV2_ATOM__Path);
				atom__float = plugin->MapURI(LV2_ATOM__Float);
				atom_Int = plugin->MapURI(LV2_ATOM__Int);
				atom_Sequence = plugin->MapURI(LV2_ATOM__Sequence);
				atom__URID = plugin->MapURI(LV2_ATOM__URID);
				atom_eventTransfer = plugin->MapURI(LV2_ATOM__eventTransfer);
				patch__Get = plugin->MapURI(LV2_PATCH__Get);
				patch__Set = plugin->MapURI(LV2_PATCH__Set);
				patch_Put = plugin->MapURI(LV2_PATCH__Put);
				patch_body = plugin->MapURI(LV2_PATCH__body);
				patch_subject = plugin->MapURI(LV2_PATCH__subject);
				patch__property = plugin->MapURI(LV2_PATCH__property);
				patch_accept = plugin->MapURI(LV2_PATCH__accept);
				patch__value = plugin->MapURI(LV2_PATCH__value);
				units__Frame = plugin->MapURI(LV2_UNITS__frame);
				param_uiState = plugin->MapURI(TOOB_WARMER_URI  "#uiState");
				param_uiData = plugin->MapURI(TOOB_WARMER_URI  "#data");
				waveShapeRequest1 = plugin->MapURI(WAVESHAPE_REQUEST_URI "1");
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
			LV2_URID patch_Put;
			LV2_URID patch_body;
			LV2_URID patch_subject;
			LV2_URID patch__property;
			LV2_URID patch__value;
			LV2_URID param_uiState;
			LV2_URID param_uiData;
			LV2_URID waveShapeRequest1;
		};

		Uris uris;

		int32_t peakDelay = 0;
		float peakValue = 0;
	private:
		LV2_Atom_Forge_Ref WriteWaveShape(LV2_URID propertyUrid,GainSection *pGain);

		void OnPatchGet(LV2_URID propertyUrid);

		float CalculateFrequencyResponse(float f);

		void SetProgram(uint8_t programNumber);
		LV2_Atom_Forge_Ref WriteFrequencyResponse();
		void WriteUiState();
	protected:
		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }
	public:
		void OnMidiCommand(int cmd0, int cmd1, int cmd2);

	public:
		static Lv2Plugin* Create(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features)
		{
			return new ToobWarmer(rate, bundle_path, features);
		}



		ToobWarmer(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);
		virtual ~ToobWarmer();

	public:
		static const char* URI;
	protected:
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();
	};
}
