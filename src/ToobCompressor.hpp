// Copyright (c) Robin E. R. Davies
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
#include "ToobCompressorInfo.hpp"
#include "ControlDezipper.h"

using namespace lv2c::lv2_plugin;
using namespace compressor_plugin;
using namespace toob;

#include "Filters/LowPassFilter.h"
#include "Filters/HighPassFilter.h"
#include "Filters/ShelvingFilter.h"

struct StereoResult {
    float left;
    float right;
};

class CompressorChannel {
public:
    void Initialize(double sampleRate);

    void Reset();

    float Tick(float value);
    StereoResult Tick(float left, float right);
private:

    HighPassFilter lowCut;
    HighPassFilter lowCut2;
    ShelvingFilter highShelf2;


};

class ToobCompressor : public compressor_plugin::ToobCompressorBase
{
public:
	using super = compressor_plugin::ToobCompressorBase;

	static Lv2Plugin *Create(double rate,
							 const char *bundle_path,
							 const LV2_Feature *const *features)
	{
		return new ToobCompressor(rate, bundle_path, features);
	}
	ToobCompressor(double rate,
				   const char *bundle_path,
				   const LV2_Feature *const *features);

	virtual ~ToobCompressor();

	static constexpr const char *URI = "http://two-play.com/plugins/toob-compressor";

protected:

	virtual void Compressor(uint32_t n_samples);

	virtual void Run(uint32_t n_samples) override;

	virtual void Activate() override;
	virtual void Deactivate() override;
private:
    CompressorChannel compressor;
};

