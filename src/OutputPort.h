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

#include <cstddef>
#include <cmath>
#include "LsNumerics/LsMath.hpp"

class OutputPort
{
private:
	float* pOut = 0;
	float defaultValue;
public:
	OutputPort(float defaultValue = 0)
	{
		this->defaultValue = defaultValue;
	}
	void SetData(void* data)
	{
		if (pOut != nullptr)
		{
			defaultValue = *pOut;
		}
		pOut = (float*)data;
		if (pOut != nullptr)
		{
			*pOut = defaultValue;
		}
	}

	void SetValue(float value)
	{
		if (pOut != 0)
		{
			*pOut = value;
		}
		else {
			defaultValue = value;
		}
	}
};

class RateLimitedOutputPort {
private:
	float* pOut = 0;
	float updateRateHz;
	size_t updateRate;
	size_t sampleCount = 0;
	float lastValue = 0;
public:
	RateLimitedOutputPort(float updateRateHz = 30.0f)
	:updateRateHz(updateRateHz)
	{
	}
	void SetSampleRate(double sampleRate)
	{
		updateRate = (size_t)(sampleRate/updateRateHz);
	}
	void Reset(double value) {
		sampleCount = 0;
		lastValue = value;
		if (pOut)
		{
			*pOut = value;
		}

	}
	void SetData(void* data)
	{
		pOut = (float*)data;
		if (pOut != nullptr)
		{
			*pOut = lastValue;
		}
	}

	void SetValue(float value)
	{
		lastValue = value;
		if (++sampleCount >= updateRate)
		{
			sampleCount -= updateRate;
			if (pOut)
			{
				*pOut = lastValue;
			}
		}
	}
	void SetValue(float value, size_t n_values)
	{
		lastValue = value;
		sampleCount += n_values;
		if (sampleCount >= updateRate)
		{
			sampleCount -= updateRate;
			if (pOut)
			{
				*pOut = lastValue;
			}
		}
	}
};
class VuOutputPort {
private:
	float* pOut = 0;
	float minDb, maxDb;
	size_t updateRate;
	size_t sampleCount = 0;
	float maxValue = 0;
public:
	VuOutputPort(float minDb, float maxDb)
	{
		this->minDb = minDb;
		this->maxDb = maxDb;
	}
	void SetSampleRate(double sampleRate)
	{
		updateRate = (size_t)(sampleRate/30);
	}
	void Reset() {
		// update 30 times a second.
		sampleCount = 0;
		maxValue = 0;
		if (pOut)
		{
			*pOut = minDb;
		}

	}
	void SetData(void* data)
	{
		this->minDb = minDb;
		this->maxDb = maxDb;
		pOut = (float*)data;
		if (pOut != nullptr)
		{
			*pOut = minDb;
		}
	}

	void AddValue(float value)
	{
		auto t = std::abs(value);
		if (t > maxValue)
		{
			maxValue = t;
		}
		if (++sampleCount >= updateRate)
		{
			sampleCount -= updateRate;
			if (pOut)
			{
				float value = LsNumerics::Af2Db(maxValue);
				if (value < minDb) value = minDb;
				if (value > maxDb) value = maxDb;
				*pOut = value;
			}
			maxValue = 0;
		}
	}
	void AddValues(size_t count, float*values)
	{

		for (size_t i = 0; i < count; ++i)
		{
			auto t = std::abs(values[i]);
			if (t > maxValue)
			{
				maxValue = t;
			}
		}
		sampleCount += count;
		if (sampleCount >= updateRate)
		{
			sampleCount -= updateRate;
			if (pOut)
			{
				float value = LsNumerics::Af2Db(maxValue);
				if (value < minDb) value = minDb;
				if (value > maxDb) value = maxDb;
				*pOut = value;
			}
			maxValue = 0;
		}
	}
};