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

#include "../std.h"

#include "AudioFilter3.h"
#include <complex>
#include <math.h>
#include <cmath>
#include <cstring>
#include <memory>
#include "../LsNumerics/LsMath.hpp"

using namespace std;

using namespace TwoPlay;

void AudioFilter3::Reset()
{
	memset(x, 0, sizeof(x));
	memset(y, 0, sizeof(y));
	memset(xR, 0, sizeof(xR));
	memset(yR, 0, sizeof(yR));
}

void AudioFilter3::Disable()
{
	// set to identity IIF.
	this->zTransformCoefficients.Disable();

}

static constexpr double PI = LsNumerics::Pi;
static constexpr double TWO_PI = PI*2;

void AudioFilter3::BilinearTransform(float frequency, const FilterCoefficients3& prototype, FilterCoefficients3* result)
{
	double w0 = frequency * TWO_PI;
	double k = 1 / std::tan(w0 * T * 0.5);
	double k2 = k * k;
	double k3 = k2*k;


	double b0 = prototype.b[0] + prototype.b[1] * k + prototype.b[2] * k2 + prototype.b[3] * k3;
	double b1 = 3 * prototype.b[0] + 1 * prototype.b[1] - 1 * prototype.b[2] * k2 -3* prototype.b[3] * k3;
	double b2 = 3* prototype.b[0] - prototype.b[1] * k - prototype.b[2] * k2 + 3*prototype.b[3] * k3;
	double b3 = prototype.b[0] - prototype.b[1] * k + prototype.b[2] * k2 - prototype.b[3] * k3;

	double a0 = prototype.a[0] + prototype.a[1] * k + prototype.a[2] * k2 + prototype.a[3] * k3;
	double a1 = 3 * prototype.a[0] + prototype.a[1]- prototype.a[2] * k2 - 3* prototype.a[3] * k3;
	double a2 = 3 *prototype.a[0] - prototype.a[1] * k - prototype.a[2] * k2 +3* prototype.a[3] * k3;
	double a3 = prototype.a[0] - prototype.a[1] * k + prototype.a[2] * k2 - prototype.a[3] * k3;

	// to causitive form, and normalize.
	double scale = 1.0 / a3;

	result->a[0] = a3 * scale;
	result->a[1] = a2 * scale;
	result->a[2] = a1 * scale;
	result->a[3] = a0 * scale;
	result->b[0] = b3 * scale;
	result->b[1] = b2 * scale;
	result->b[2] = b1 * scale;
	result->b[3] = b0 * scale;

}

double AudioFilter3::GetFrequencyResponse(float frequency)
{
	double w0 = frequency *T *TWO_PI;

	complex<double> ejw = std::exp(std::complex<double>(0.0, w0));
	complex<double> ejw2 = ejw * ejw;
	complex<double> ejw3 = ejw2 * ejw;

	complex<double> result =
		(
			zTransformCoefficients.b[0]
			+ zTransformCoefficients.b[1] * ejw
			+ zTransformCoefficients.b[2] * ejw2
			+ zTransformCoefficients.b[3] * ejw3
			) 
		/ (
			zTransformCoefficients.a[0]
			+ zTransformCoefficients.a[1] * ejw
			+ zTransformCoefficients.a[2] * ejw2
			+ zTransformCoefficients.a[3] * ejw3
				);



	return std::abs(result);


}