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
#include "LsNumerics/Freeverb.hpp"



#define TOOB_FREEVERB_URI "http://two-play.com/plugins/toob-freeverb"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif



namespace toob {

	class ToobFreeverb : public Lv2Plugin {
	private:
		enum class PortId {
            BYPASS,
			DRYWET,
			ROOMSIZE,
			DAMPING,
            TAILS,
			AUDIO_INL,
			AUDIO_INR,
			AUDIO_OUTL,
			AUDIO_OUTR,
		};

		const float*bypass = nullptr;
		const float*dryWet = nullptr;
		const float *roomSize= nullptr;
		const float *damping = nullptr;
        const float *tails = nullptr;
		const float*inL = nullptr;
		const float*inR = nullptr;
		float*outL = nullptr;
		float*outR = nullptr;

		bool bypassValue = true;
        bool tailsValue = true;
		float dryWetValue = -1;
		float roomSizeValue = -1;
		float dampingValue = -1;


		Freeverb freeverb;
		double rate = 44100;
		std::string bundle_path;

		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }

	public:
		static Lv2Plugin* Create(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features)
		{
			return new ToobFreeverb(rate, bundle_path, features);
		}
		ToobFreeverb(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);

	public:
		static const char* URI;
	protected:
		virtual void ConnectPort(uint32_t port, void* data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();

    };

}// namespace toob