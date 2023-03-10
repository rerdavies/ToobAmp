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

// InputStage.h 

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
#include "NoiseGate.h"
#include "GainStage.h"
#include "GainSection.h"
#include "DbDezipper.h"
#include "SagProcessor.h"
#include "Filters/ChebyshevDownsamplingFilter.h"



#define POWER_STAGE_2_URI "http://two-play.com/plugins/toob-power-stage-2"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif
#define WAVESHAPE_REQUEST_URI TOOB_URI "#waveShape"

namespace TwoPlay {
	class PowerStage2 : public Lv2Plugin {
	private:
		enum class PortId {
			TRIM1 = 0,
			LOCUT1,
			HICUT1,
			SHAPE1,
			GAIN1,
			BIAS1,

			TRIM2,
			LOCUT2,
			HICUT2,
			SHAPE2,
			GAIN2,
			BIAS2,
			GAIN2_ENABLE, 


			TRIM3,
			LOCUT3,
			HICUT3,
			SHAPE3,
			GAIN3,
			BIAS3,
			GAIN3_ENABLE, 

			SAG,   
			SAGD,
			MASTER,

			AUDIO_IN, 
			AUDIO_OUT,
			CONTROL_IN,
			NOTIFY_OUT,

			// Non-gui controls
			SAGF,    //21
		};

		double rate;
		std::string bundle_path;

		GainSection gain1;
		GainSection gain2;
		GainSection gain3;
		SagProcessor sagProcessor;

		RangedInputPort gain2_enable = RangedInputPort(0.0f,1.0f);
		RangedInputPort gain3_enable = RangedInputPort(0.0f,1.0f);
		RangedDbInputPort master = RangedDbInputPort(-60.0f, 30.0f);

		const float* input = NULL;
		float* output = NULL;

		LV2_Atom_Sequence* controlIn = NULL;
		LV2_Atom_Sequence* notifyOut = NULL;

		DbDezipper masterVolumeDezipped;

		ChebyshevDownsamplingFilter upsamplingFilter;
		ChebyshevDownsamplingFilter downsamplingFilter;
		float *upsampledInputBuffer = nullptr;
		float *upsampledOutputBuffer = nullptr;
		float lastvalue = 0;


		uint64_t frameTime = 0;

		int64_t updateSampleDelay;
		int64_t updateSamples = 0;

		LV2_Atom_Forge       forge;        ///< Forge for writing atoms in run thread



		struct Uris {
		public:
			void Map(Lv2Plugin* plugin)
			{
				pluginUri = plugin->MapURI(POWER_STAGE_2_URI);

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
				unitsFrame = plugin->MapURI(LV2_UNITS__frame);
				param_uiState = plugin->MapURI(POWER_STAGE_2_URI  "#uiState");
				param_uiData = plugin->MapURI(POWER_STAGE_2_URI  "#data");
				waveShapeRequest1 = plugin->MapURI(WAVESHAPE_REQUEST_URI "1");
				waveShapeRequest2 = plugin->MapURI(WAVESHAPE_REQUEST_URI "2");
				waveShapeRequest3 = plugin->MapURI(WAVESHAPE_REQUEST_URI "3");
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
			LV2_URID param_uiState;
			LV2_URID param_uiData;
			LV2_URID waveShapeRequest1;
			LV2_URID waveShapeRequest2;
			LV2_URID waveShapeRequest3;
		};

		Uris uris;

		int32_t peakDelay = 0;
		float peakValue = 0;
	private:
		LV2_Atom_Forge_Ref WriteWaveShape(LV2_URID propertyUrid,GainSection *pGain);

		void OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object);

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
			return new PowerStage2(rate, bundle_path, features);
		}



		PowerStage2(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);
		virtual ~PowerStage2();

	public:
		static const char* URI;
	protected:
		virtual void ConnectPort(uint32_t port, void* data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();
	};
}
