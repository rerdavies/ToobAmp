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


namespace toob {
	class FilterCoefficients2 {
	public:
		FilterCoefficients2()
		{
			a[0] = a[1] = a[2] = 0;
			b[0] = b[1] = b[2] = 0;
		}
		FilterCoefficients2(double b0, double b1, double b2,double a0, double a1, double a2)
		{
			a[0] = a0; a[1] = a1; a[2] = a2;
			b[0] = b0; b[1] = b1; b[2] = b2;
		}
		FilterCoefficients2(const FilterCoefficients2& other)
		{
			a[0] = other.a[0];
			a[1] = other.a[1];
			a[2] = other.a[2];
			b[0] = other.b[0];
			b[1] = other.b[1];
			b[2] = other.b[2];
		}
		void Disable()
		{
			a[0] = b[0] = 1;
			a[1] = a[2] = b[1] = b[2] = 0;
		}

		double a[3];
		double b[3];

		FilterCoefficients2 HighPass() {
			FilterCoefficients2 result;

			result.a[0] = a[2]; result.a[1] = a[1]; result.a[2] = a[0];
			result.b[0] = b[2]; result.b[1] = b[1]; result.b[2] = b[0];
			return result;
		}
	};
}