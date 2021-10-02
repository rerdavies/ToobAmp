#pragma once

#include "std.h"
#include <cmath>

namespace TwoPlay {


	class MathInternal {
	public:
		static float log10;
	};
	inline static float Af2Db(float value)
	{
		return 20.0f*std::log10(value);
	}
	inline float Db2Af(float value)
	{
		if (value < -192) return 0;
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