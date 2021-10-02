#pragma once

#include <limits>
#include "ToobMath.h"

namespace TwoPlay {
	class InputPort {
	private:
		const float* pData = NULL;
		float lastValue = std::numeric_limits<float>::min();

	public:
		void SetData(void* data) {
			pData = (float*)data;
		}
		bool HasChanged()
		{
			if (pData == NULL) return false;
			return (*pData != lastValue);
		}

		float GetValue()
		{
			return lastValue = *pData;
		}
	};

	class RangedInputPort {
	private:
		float minValue, maxValue;
		const float* pData = NULL;
		float lastValue = std::numeric_limits<float>::min();

	private:
		float ClampedValue()
		{
			float v = *pData;
			if (v < minValue) v = minValue;
			if (v > maxValue) v = maxValue;
			return v;
		}
	public:
		RangedInputPort(float minValue, float maxValue)
		{
			this->minValue = minValue;
			this->maxValue = maxValue;
		}
		float GetMaxValue() { return this->maxValue; }
		float GetMinValue() { return this->minValue; }

		void SetData(void* data) {
			pData = (float*)data;
		}
		bool HasChanged()
		{
			// fast path for well-behaved hosts that trim inputs.
			if (*pData == lastValue) return false; 

			// make sure the host hasn't given us an out of range value.
			return ClampedValue() != lastValue;
		}

		float GetValue()
		{
			return lastValue = ClampedValue();
		}

	};

	class RangedDbInputPort {
	private:
		float minValue, maxValue;
		const float* pData = NULL;
		float lastValue = std::numeric_limits<float>::min();
		float lastAfValue = 0;

	private:
		float ClampedValue()
		{
			float v = *pData;
			if (v < minValue) v = minValue;
			if (v > maxValue) v = maxValue;
			return v;
		}
	public:
		RangedDbInputPort(float minValue, float maxValue)
		{
			this->minValue = minValue;
			this->maxValue = maxValue;
		}
		float GetMinDb() const { return minValue; }
		float GetMaxDb() const{ return maxValue; }

		void SetData(void* data) {
			pData = (float*)data;
		}
		bool HasChanged()
		{
			if (*pData == lastValue) return false; // fast path for well-behaved hosts.

			// make sure the host hasn't given us an out of range value.
			return ClampedValue() != lastValue;
		}
		float GetDb()
		{
			if (HasChanged())
			{
				lastValue = ClampedValue();
				lastAfValue = Db2Af(lastValue);
			}
			return lastValue;
		}

		float GetAf()
		{
			if (HasChanged())
			{
				lastValue = ClampedValue();
				lastAfValue = Db2Af(lastValue);
			}
			return lastAfValue;
		}

	};

	class SteppedInputPort  {
	private:
		float *pData;
		float lastValue;
		int minValue,maxValue;
		int value;
	public:
		SteppedInputPort(int minValue, int maxValue)
		{
			this->minValue = minValue;
			this->maxValue = maxValue;
			this->lastValue = std::numeric_limits<float>::max();

		}
		bool HasChanged()
		{
			if (pData == NULL) return false;
			return (*pData != lastValue);
		}

		int GetValue()
		{
			lastValue = (*pData);
			int result = (int)(lastValue+0.5);
			if (result < minValue) return minValue;
			if (result > maxValue) return  maxValue;
			return result;
		}
		void SetData(void*pData)
		{
			this->pData = (float*)pData;
		}

	};

}