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

#include "FilterCoefficients2.h"
#include "../InputPort.h"


namespace toob {
	using namespace ::LsNumerics;

	class AudioFilter2 {
	protected:
		FilterCoefficients2 prototype;
		FilterCoefficients2 zTransformCoefficients;
		float cutoffFrequency;
	private:
		double T = 1;
		float referenceFrequency;
		double x[2];
		double y[2];
		double xR[2];
		double yR[2];
		float disabledFrequency = -1;
	protected:
		FilterCoefficients2*GetPrototype() { return &prototype; }

	public:
		RangedInputPort Frequency;
	protected:
		AudioFilter2()
		: Frequency(0.0f,0.0f)
		{
			disabledFrequency = -1;
			Reset();			
		}
	public:
		AudioFilter2(const FilterCoefficients2& prototype, float minFrequency,float maxFrequency, float disableFrequency = -1)
		: Frequency(minFrequency,maxFrequency)
		{
			this->prototype = prototype;
			this->disabledFrequency = disableFrequency;
			Reset();
			this->referenceFrequency = 1.0f;
		}

		AudioFilter2(const FilterCoefficients2& prototype, float referenceFreqency = 1.0f)
		: Frequency(0.0f,0.0f)
		{
			disabledFrequency = -1;
			Reset();
			this->prototype = prototype;
			this->referenceFrequency = referenceFreqency;
		}
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
			double x1 = x[0];
			double x2 = x[1];
			double y1 = y[0];
			double y2 = y[1];

			double y0 = LsNumerics::Undenormalize(x0 * (zTransformCoefficients.b[0])
				+ x1 * zTransformCoefficients.b[1]
				+ x2 * zTransformCoefficients.b[2]
				- (
					y1* zTransformCoefficients.a[1]
					+ y2* zTransformCoefficients.a[2]
					));
			y[0] = y0;
			y[1] = y1;
			x[0] = x0;
			x[1] = x1;

			return y0;
		}
		inline double TickR(double x0)
		{
			double x1 = xR[0];
			double x2 = xR[1];
			double y1 = yR[0];
			double y2 = yR[1];

			double y0 = Undenormalize(x0 * (zTransformCoefficients.b[0])
				+ x1 * zTransformCoefficients.b[1]
				+ x2 * zTransformCoefficients.b[2]
				- (
					y1* zTransformCoefficients.a[1]
					+ y2* zTransformCoefficients.a[2]
					));
			yR[0] = y0;
			yR[1] = y1;
			xR[0] = x0;
			xR[1] = x1;

			return y0;
		}

		double GetFrequencyResponse(float frequency);
	protected:
		void BilinearTransform(float frequency, const FilterCoefficients2& prototype, FilterCoefficients2* result);

	};
}