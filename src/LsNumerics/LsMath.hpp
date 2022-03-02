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

#include <cmath>
#include <cstdint>

namespace LsNumerics {

    constexpr double Pi = 3.141592653589793238462643383279502884;


	namespace MathInternal {
		const float log10 = 2.302585093f; //std::log(10);
	};

	// inputValue: a value between zero and one.
	// Returns:
	//    a logarithmically tapered value between 0.01 and 1, having a 
	//    value of 0.1 for an input of 0.5. A common taper curve used by most amp
	//    manufacturers (except for Fender).

	inline double AudioTaper(double inputValue)
	{
		return std::exp(MathInternal::log10*(inputValue-1)*2);
	}
	const float MIN_DB = -200;
	const float  MIN_DB_AMPLITUDE = 1e-10f;

	inline float Af2Db(float value)
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

	constexpr int MIDI_A440_NOTE = 69;

	inline double FrequencyToMidiNote(double frequency, double aReference = 440.0)
	{
		return 12*std::log2(frequency/aReference) + MIDI_A440_NOTE;
	}



};