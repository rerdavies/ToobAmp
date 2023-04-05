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

#include "AudioFilter2.h"
#include <complex>
#include <math.h>
#include <cmath>
#include <cstring>
#include <memory>

using namespace std;

using namespace toob;

void AudioFilter2::Reset()
{
	memset(x, 0, sizeof(x));
	memset(y, 0, sizeof(y));
	memset(xR, 0, sizeof(xR));
	memset(yR, 0, sizeof(yR));
}

void AudioFilter2::Disable()
{
	// set to identity IIF.
	this->zTransformCoefficients.Disable();

}

static constexpr double PI = 3.14159265358979323846;
static constexpr double TWO_PI = PI*2;

void AudioFilter2::BilinearTransform(float frequency, const FilterCoefficients2& prototype, FilterCoefficients2* result)
{
	double w0 = frequency * TWO_PI;
	double k = 1 / std::tan(w0 * T * 0.5);
	double k2 = k * k;

	double b0 = prototype.b[0] - prototype.b[1] * k + prototype.b[2] * k2;
	double b1 = 2 * prototype.b[0] - 2 * prototype.b[2] * k2;
	double b2 = prototype.b[0] + prototype.b[1] * k + prototype.b[2] * k2;

	double a0 = prototype.a[0] - prototype.a[1] * k + prototype.a[2] * k2;
	double a1 = 2 * prototype.a[0] - 2 * prototype.a[2] * k2;
	double a2 = prototype.a[0] + prototype.a[1] * k + prototype.a[2] * k2;

	// causitive form, and normalize.
	double scale = 1.0 / a2;

	result->a[0] = a2 * scale;
	result->a[1] = a1 * scale;
	result->a[2] = a0 * scale;
	result->b[0] = b2 * scale;
	result->b[1] = b1 * scale;
	result->b[2] = b0 * scale;

}

double AudioFilter2::GetFrequencyResponse(float frequency)
{
	double w0 = frequency *T *TWO_PI;

	complex<double> ejw = std::exp(complex<double>(0.0, w0));
	complex<double> ejw2 = ejw * ejw;


	complex<double> result =
		(
			zTransformCoefficients.b[0]
			+ zTransformCoefficients.b[1] * ejw
			+ zTransformCoefficients.b[2] * ejw2
			) 
		/ (
			zTransformCoefficients.a[0]
			+ zTransformCoefficients.a[1] * ejw
			+ zTransformCoefficients.a[2] * ejw2

				);



	return std::abs(result);


}