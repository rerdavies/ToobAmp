/*
Copyright (c) 2021 Robin Davies

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
// CabSim.h 

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

#include "Lv2Plugin.h"

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "Filters/AudioFilter2.h"
#include "Filters/ShelvingLowCutFilter2.h"
#include "CombFilter.h"
#include "NoiseGate.h"
#include "GainStage.h"



#define CAB_SIM_URI "http://two-play.com/plugins/toob-cab-sim"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

namespace TwoPlay {
	class CabSim : public Lv2Plugin {
	private:
		enum class PortId {
			LOCUT = 0,
			BRIGHT,
			BRIGHTF,
			HICUT,
			COMB,
			COMBF,
			TRIM,
			AUDIO_IN,
			AUDIO_OUT,
			CONTROL_IN,
			NOTIFY_OUT,
		};

		double rate;
		std::string bundle_path;

		RangedDbInputPort trim = RangedDbInputPort(-60.0f, 30.0f);
		RangedDbInputPort bright = RangedDbInputPort(0, 25.0f);
		RangedInputPort brightf = RangedInputPort(1000.0f, 8000.0f);
		
		RangedDbInputPort gateT = RangedDbInputPort(-80.0f, -20.0f);
		RangedDbInputPort boost = RangedDbInputPort(0, 1.0f);

		static FilterCoefficients2 LOWPASS_PROTOTYPE;
		AudioFilter2 highCutFilter = AudioFilter2(LOWPASS_PROTOTYPE,2000.0,13000.0f,13000.0f);
		static FilterCoefficients2 HIPASS_PROTOTYPE;
		AudioFilter2 loCutFilter = AudioFilter2(HIPASS_PROTOTYPE,30.0f,300.0f,30.0f);
		ShelvingLowCutFilter2 brightFilter = ShelvingLowCutFilter2();
		CombFilter combFilter;


		const float* inputL = NULL;;
		float* outputL = NULL;;

		LV2_Atom_Sequence* controlIn = NULL;
		LV2_Atom_Sequence* notifyOut = NULL;
		uint64_t frameTime = 0;

		bool responseChanged = true;
        bool patchGet = false;
		int64_t updateSampleDelay;
		uint64_t updateMsDelay;

		int64_t updateSamples = 0;
		uint64_t updateMs = 0;


		int programNumber;

		LV2_Atom_Forge       forge;        ///< Forge for writing atoms in run thread

		void HandleEvent(LV2_Atom_Event* event);

		struct Uris {
		public:
			void Map(Lv2Plugin* plugin)
			{
				pluginUri = plugin->MapURI(CAB_SIM_URI);

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
				param_frequencyResponseVector = plugin->MapURI(TOOB_URI  "#frequencyResponseVector");
				param_uiState = plugin->MapURI(CAB_SIM_URI  "#uiState");
			}
			LV2_URID patch_accept;

			LV2_URID frequencyRequest;
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
			LV2_URID param_uiState;
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
		void WriteUiState();
	protected:
		virtual void OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object);
		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }
	public:
		void OnMidiCommand(int cmd0, int cmd1, int cmd2);

	public:
		static Lv2Plugin* Create(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features)
		{
			return new CabSim(rate, bundle_path, features);
		}



		CabSim(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);
		virtual ~CabSim();

	public:
		static const char* URI;
	protected:
		virtual void ConnectPort(uint32_t port, void* data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();
	};
};
