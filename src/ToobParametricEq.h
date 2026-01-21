/*
 *   Copyright (c) 2026 Robin E. R. Davies
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

#include <lv2_plugin/Lv2Plugin.hpp>


#define DEFINE_LV2_PLUGIN_BASE
#include "ToobParametricEqInfo.hpp"

#include "ParametricEq.hpp"


#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif


namespace toob {
	using namespace LsNumerics;

class ToobParametricEq : public ToobParametricEqBase {

	private:
        using super = ToobParametricEqBase;

    public:
        static constexpr const char *MONO_URI = "http://two-play.com/plugins/toob-parametric-eq";
        static constexpr const char *STEREO_URI = "http://two-play.com/plugins/toob-parametric-eq-stereo";


		LV2_Atom_Sequence* controlIn = NULL;
		LV2_Atom_Sequence* notifyOut = NULL;
		uint64_t frameTime = 0;

		bool responseChanged = true;
        bool patchGet = false;
		int64_t updateSampleDelay;
		uint64_t updateMsDelay;

		int64_t updateSamples = 0;
		uint64_t updateMs = 0;



		struct Uris {
		public:
			void Map(Lv2Plugin* plugin)
			{
				param_frequencyResponseVector = plugin->MapURI(TOOB_URI  "#frequencyResponseVector");
                patch__Get = plugin->MapURI(LV2_PATCH__Get);
                patch__Set = plugin->MapURI(LV2_PATCH__Set);
                patch__property = plugin->MapURI(LV2_PATCH__property);
                patch__accept = plugin->MapURI(LV2_PATCH__accept);
                patch__value = plugin->MapURI(LV2_PATCH__value);
                atom__URID = plugin->MapURI(LV2_ATOM__URID);
                atom__Float = plugin->MapURI(LV2_ATOM__Float);
                atom__Int = plugin->MapURI(LV2_ATOM__Int);
                atom__String = plugin->MapURI(LV2_ATOM__String);
                atom__Path = plugin->MapURI(LV2_ATOM__Path);
                units__frame = plugin->MapURI(LV2_UNITS__frame);
			}
			LV2_URID param_frequencyResponseVector;

            LV2_URID patch;
            LV2_URID patch__Get;
            LV2_URID patch__Set;
            LV2_URID patch__property;
            LV2_URID patch__accept;
            LV2_URID patch__value;
            LV2_URID atom__URID;
            LV2_URID atom__Float;
            LV2_URID atom__Int;
            LV2_URID atom__String;
            LV2_URID atom__Path;
            LV2_URID units__frame;

		};

		Uris uris;


		FilterResponse filterResponse;
        float rate = 44100.0f;

	protected:
		virtual void OnPatchGet(LV2_URID propertyUrid);

	private:
        DbDezipper gainDezipper;

	protected:
		bool isStereo = false;
        ParametricEq eq;


		bool UpdateControls();

		float CalculateFrequencyResponse(float f);

		void WriteFrequencyResponse();

	protected:
		double getRate() { return rate; }

	public:
		static Lv2Plugin* Create(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features)
		{
			return new ToobParametricEq(rate, bundle_path, features);
		}



		ToobParametricEq(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);
	protected:
		ToobParametricEq(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features, 
			bool isStereo
		);

	public:
		virtual ~ToobParametricEq();

	protected:
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();
	};
}
