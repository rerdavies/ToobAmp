/*
 *   Copyright (c) 2021 Robin E. R. Davies
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
#include <cmath>

namespace TwoPlay {


	namespace MathInternal {
		const float log10 = 2.302585093f; //std::log(10);
	};
	const float MIN_DB = -200;
	const float  MIN_DB_AMPLITUDE = 1e-10f;

	inline static float Af2Db(float value)
	{
		if (value < MIN_DB_AMPLITUDE) return MIN_DB;
		return 20.0f*std::log10(value);
	}
	inline float Db2Af(float value)
	{
		if (value < MIN_DB) return 0;
		return std::exp(value*(MathInternal::log10*0.05f));
	}

	uint32_t NextPowerOfTwo(uint32_t value);

	inline double Undenormalize(double value)
	{
		return 1E-18 +value+ 1E-18;
	}
	inline float Undenormalize(float value)
	{
		return 1E-6f +value+ 1E-6f;
	}



};