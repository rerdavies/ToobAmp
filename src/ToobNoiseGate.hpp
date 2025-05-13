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
#include "ToobNoiseGateInfo.hpp"
#include "ControlDezipper.h"

using namespace lv2c::lv2_plugin;
using namespace noise_gate_plugin;
using namespace toob;


class ToobNoiseGate : public noise_gate_plugin::ToobNoiseGateBase
{
public:
	using super = noise_gate_plugin::ToobNoiseGateBase;

	static Lv2Plugin *Create(double rate,
							 const char *bundle_path,
							 const LV2_Feature *const *features)
	{
		return new ToobNoiseGate(rate, bundle_path, features);
	}
	ToobNoiseGate(double rate,
				   const char *bundle_path,
				   const LV2_Feature *const *features);

	virtual ~ToobNoiseGate();

protected:

	virtual void Mix(uint32_t n_samples);

	virtual void Run(uint32_t n_samples) override;

	virtual void Activate() override;
	virtual void Deactivate() override;
private:
	enum class GateState {
		Idle,
		Attack,
		Hold,
		Release	
	};

	GateState gateState = GateState::Idle;
	int64_t samplesRemaining = 32768;

	double currentDb = -96;
	double dxCurrentDb = 0;

	void ResetGateState();

	void UpdateControls();

	double thresholdValue = 0;
	double hysteresisValue = 0;
	double reductionValue = 0;
	size_t attackSamples = 1;
	size_t holdSamples = 100;
	size_t releaseSamples = 10000;
};

