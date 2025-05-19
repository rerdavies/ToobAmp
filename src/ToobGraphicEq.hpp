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
#include "ToobGraphicEqInfo.hpp"
#include "ControlDezipper.h"
#include "HoltersGraphicEq.hpp"

using namespace lv2c::lv2_plugin;
using namespace graphiceq_plugin;
using namespace toob;
using namespace toob::holters_graphic_eq;


class ToobGraphicEq : public graphiceq_plugin::ToobGraphicEqBase
{
public:
	using super = graphiceq_plugin::ToobGraphicEqBase;

	static Lv2Plugin *Create(double rate,
							 const char *bundle_path,
							 const LV2_Feature *const *features)
	{
		return new ToobGraphicEq(rate, bundle_path, features);
	}
	ToobGraphicEq(double rate,
				   const char *bundle_path,
				   const LV2_Feature *const *features);

	virtual ~ToobGraphicEq();

protected:

	virtual void Run(uint32_t n_samples) override;

	virtual void Activate() override;
	virtual void Deactivate() override;
private:
	toob::holters_graphic_eq::GraphicEq graphicEq;
	
};

