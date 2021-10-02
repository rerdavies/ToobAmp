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

#include "FilterCoefficients3.h"
#include "../InputPort.h"


namespace TwoPlay {

	class AudioFilter3 {
	protected:
		FilterCoefficients3 prototype;
		FilterCoefficients3 zTransformCoefficients;
		float cutoffFrequency;
		double T = 1;
	private:
		float referenceFrequency;
		double x[3];
		double y[3];
		double xR[3];
		double yR[3];
		float disabledFrequency = -1;
	protected:
		const FilterCoefficients3*GetPrototype() { return &prototype; }

	public:
		RangedInputPort Frequency;
	protected:
		AudioFilter3()
		: Frequency(0.0f,0.0f)
		{
			disabledFrequency = -1;
		}
	public:
		AudioFilter3(const FilterCoefficients3& prototype, float minFrequency,float maxFrequency, float disableFrequency = -1)
		: Frequency(minFrequency,maxFrequency)
		{
			this->prototype = prototype;
			this->disabledFrequency = disableFrequency;
			Reset();
			this->referenceFrequency = 1.0f;
		}

		AudioFilter3(const FilterCoefficients3& prototype, float referenceFreqency = 1.0f)
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
			double x3 = x[2];
			double y1 = y[0];
			double y2 = y[1];
			double y3 = y[2];

			double y0 = Undenormalize(x0 * (zTransformCoefficients.b[0])
				+ x1 * zTransformCoefficients.b[1]
				+ x2 * zTransformCoefficients.b[2]
				+ x3 * zTransformCoefficients.b[3]
				- (
					y1* zTransformCoefficients.a[1]
					+ y2* zTransformCoefficients.a[2]
					+ y3* zTransformCoefficients.a[3]
					));
			y[0] = y0;
			y[1] = y1;
			y[2] = y2;
			x[0] = x0;
			x[1] = x1;
			x[2] = x2;

			return y0;
		}
		inline double TickR(double x0)
		{
			double x1 = xR[0];
			double x2 = xR[1];
			double x3 = xR[2];
			double y1 = yR[0];
			double y2 = yR[1];
			double y3 = yR[2];

			double y0 = x0 * (zTransformCoefficients.b[0])
				+ x1 * zTransformCoefficients.b[1]
				+ x2 * zTransformCoefficients.b[2]
				+ x3 * zTransformCoefficients.b[3]
				- (
					y1* zTransformCoefficients.a[1]
					+ y2* zTransformCoefficients.a[2]
					+ y3* zTransformCoefficients.a[3]
					);
			yR[0] = y0;
			yR[1] = y1;
			yR[2] = y2;
			xR[0] = x0;
			xR[1] = x1;
			xR[2] = x2;

			return y0;
		}

		double GetFrequencyResponse(float frequency);
	protected:
		void BilinearTransform(float frequency, const FilterCoefficients3& prototype, FilterCoefficients3* result);

	};
}