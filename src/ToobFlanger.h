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

#include <lv2_plugin/Lv2Plugin.hpp>

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "ControlDezipper.h"
#include "Tf2Flanger.hpp"

namespace toob
{

	class ToobFlangerBase : public Lv2Plugin
	{
	private:
		enum class PortId
		{
			MANUAL = 0,
			DEPTH,
			RATE,
			LFO,
			RES,
            DRYWET,
			AUDIO_INL,
			AUDIO_OUTL,
			AUDIO_OUTR,
		};

		const float *pManual = nullptr;
		const float *pRate = nullptr;
		const float *pDepth = nullptr;
		const float *pRes = nullptr;
		const float *pDryWet = nullptr;
		float *pLfo = nullptr;
		const float *inL = nullptr;
		float *outL = nullptr;
		float *outR = nullptr;

		float lastRes = -2;
		float lastDryWet = -2;
		float lastManual = -2;
		float lastRate = -2;
		float lastDepth = -2;

        ControlDezipper dryWetDezipper;
        
		double rate = 44100;
		std::string bundle_path;

		Tf2Flanger flanger;
		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }

		void clear();
		void updateControls();

	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobFlangerBase(rate, bundle_path, features);
		}
		ToobFlangerBase(double rate,
						const char *bundle_path,
						const LV2_Feature *const *features);


	protected:
		virtual void ConnectPort(uint32_t port, void *data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();
	};

	class ToobFlanger : public ToobFlangerBase
	{
	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobFlanger(rate, bundle_path, features);
		}
		ToobFlanger(double rate,
					const char *bundle_path,
					const LV2_Feature *const *features) : ToobFlangerBase(rate, bundle_path, features)
		{
		}
		static const char *URI;
	};
	class ToobFlangerStereo : public ToobFlangerBase
	{
	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobFlangerStereo(rate, bundle_path, features);
		}
		ToobFlangerStereo(double rate,
						  const char *bundle_path,
						  const LV2_Feature *const *features) : ToobFlangerBase(rate, bundle_path, features)
		{
		}
		static const char *URI;
	};

} // namespace toob