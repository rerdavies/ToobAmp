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

#include "FilterCoefficients.h"
#include "../InputPort.h"
#include <vector>
#include "Polynomial.h"


namespace TwoPlay {

	class AudioFilter {
	protected:
		FilterCoefficients prototype;
		FilterCoefficients zTransformCoefficients;
		float cutoffFrequency;
	private:
		double T = 1;
		float referenceFrequency;
		size_t length;
		std::vector<TwoPlay::Polynomial> bilinearTransformCoefficients;
		double *x;
		double *y;
		double *xR;
		double *yR;
		float disabledFrequency = -1;

		void Initialize(size_t length);

	protected:
		FilterCoefficients*GetPrototype() { return &prototype; }

	public:
		RangedInputPort Frequency;
	protected:
		AudioFilter(int length)
		: Frequency(0.0f,0.0f)
		{
			Initialize(length);
			disabledFrequency = -1;
		}
	public:
		AudioFilter(const FilterCoefficients& prototype, float minFrequency,float maxFrequency, float disableFrequency = -1)
		: Frequency(minFrequency,maxFrequency)
		{
			Initialize(prototype.length);

			this->prototype = prototype;
			this->disabledFrequency = disableFrequency;
			Reset();
			this->referenceFrequency = 1.0f;
		}

		AudioFilter(const FilterCoefficients& prototype, float referenceFreqency = 1.0f)
		: Frequency(0.0f,0.0f)
		{
			Initialize(prototype.length);
			disabledFrequency = -1;
			Reset();
			this->prototype = prototype;
			this->referenceFrequency = referenceFreqency;
		}
		~AudioFilter();

		bool UpdateControls()
		{
			if (Frequency.HasChanged())
			{
				float f = Frequency.GetValue();
				if (f == disabledFrequency)
				{
					this->Disable();
				} else {
					this->SetCutoffFrequency(f);
				}
				return true;
			}
			return false;
		}

		void Reset();

		void SetSampleRate(float sampleRate)
		{
			T = 1.0 / sampleRate;
		}

		void Disable();
		virtual void SetCutoffFrequency(float frequency)
		{
			this->cutoffFrequency = frequency;
			BilinearTransform(frequency, prototype, &zTransformCoefficients);
		}
		inline double Tick(double x0)
		{
			for (int i = length-1; i >=1; --i)
			{
				x[i] = x[i-1];
				y[i] = y[i-1];
			}
			x[0] = x0;

			double y0 = 0;
			for (size_t i = 0; i < length; ++i)
			{
				y0 += zTransformCoefficients.b[i]*x[i];
			}
			for (size_t i = 1; i < length; ++i)
			{
				y0 -= zTransformCoefficients.a[i]*y[i];
			}
			y[0] = y0;
			return y0;
		}
		inline double TickR(double x0)
		{
			for (size_t i = length-1; i >=1;--i)
			{
				xR[i] = xR[i-1];
				yR[i] = yR[i-1];
			}
			xR[0] = x0;

			double y0 = 0;
			for (size_t i = 0; i < length; ++i)
			{
				y0 += zTransformCoefficients.b[i]*xR[i];
			}
			for (size_t i = 1; i < length; ++i)
			{
				y0 -= zTransformCoefficients.a[i]*yR[i];
			}
			yR[0] = y0;
			return y0;
		}

		double GetFrequencyResponse(float frequency);
	protected:
		void BilinearTransform(float frequency, const FilterCoefficients& prototype, FilterCoefficients* result);

	};
}