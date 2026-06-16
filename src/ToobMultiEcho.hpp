// Copyright (c) 2025 Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#define DEFINE_LV2_PLUGIN_BASE

#include <chrono>
#include <filesystem>
#include <memory>
#include "ToobMultiEchoInfo.hpp"
#include "ToobMultiEchoStereoInfo.hpp"
#include "ControlDezipper.h"
#include "restrict.hpp"

using namespace lv2c::lv2_plugin;
using namespace multi_echo_plugin;
using namespace toob;

namespace toob_multi_echo {
    class ToobMultiEchoDelayUnit {
    public:
        ToobMultiEchoDelayUnit() = default;

        enum class Mode {
            Mono,
            Left,
            Right,
        };

        void Reset();
        void Activate(
            Mode mode,
            double sampleRate,
            RangedInputPort&delayPort, 
            RangedInputPort&levelPort,
            RangedInputPort&feedbackPort,
            RangedInputPort&panPort
        );
        void UpdateControls(
            Mode mode,
            RangedInputPort&delayPort, 
            RangedInputPort&levelPort,
            RangedInputPort&feedbackPort,
            RangedInputPort&panPort
        );
        void Run(size_t nFrames, const float *input, float *output);

    private:
        double sampleRate = 48000;
        size_t insertPosition = 0;
        size_t tap = 0;
        float level = 0;
        float feedback = 0;
        std::vector<float> delayLine;
    };
}

using namespace toob_multi_echo;

class ToobMultiEcho : public multi_echo_plugin::ToobMultiEchoStereoBase
{
public:
	using super = multi_echo_plugin::ToobMultiEchoStereoBase;

	static Lv2Plugin *Create(double rate,
							 const char *bundle_path,
							 const LV2_Feature *const *features)
	{
		return new ToobMultiEcho(rate, bundle_path, features);
	}
	ToobMultiEcho(double rate,
				   const char *bundle_path,
				   const LV2_Feature *const *features);

	virtual ~ToobMultiEcho();

	static constexpr const char *URI = "http://two-play.com/plugins/toob-multi-echo";

protected:

	virtual void Run(uint32_t n_samples) override;

	virtual void Activate() override;
	virtual void Deactivate() override;
private:
    void UpdateControls();

    void UpdateStereoDelays(
        ToobMultiEchoDelayUnit &leftDelay,
        ToobMultiEchoDelayUnit &rightDelay,
        RangedInputPort&delayPort, 
        RangedInputPort&levelPort,
        RangedInputPort&feedbackPort,
        RangedInputPort&panPort
    );

    double sampleRate = 44100;
    bool enable = true;
    float directLevel = 1.0;
    float masterLevel = 1.0;

    std::array<ToobMultiEchoDelayUnit, 4> leftDelays;
    std::array<ToobMultiEchoDelayUnit, 4> rightDelays;
    bool enabled = false;
    bool isStereo = false;

};

