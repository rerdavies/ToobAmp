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


namespace TwoPlay {
	class FilterCoefficients3 {
	public:
		FilterCoefficients3()
		{
			a[0] = a[1] = a[2] = a[3] = 0; 
			b[0] = b[1] = b[2] = b[3] = 0; 
		}
		FilterCoefficients3(double b0, double b1, double b2,double b3,double a0, double a1, double a2,double a3)
		{
			a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3;
			b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3;
		}
		FilterCoefficients3(const FilterCoefficients3& other)
		{
			a[0] = other.a[0];
			a[1] = other.a[1];
			a[2] = other.a[2];
			a[3] = other.a[3];
			b[0] = other.b[0];
			b[1] = other.b[1];
			b[2] = other.b[2];
			b[3] = other.b[3];
		}
		void Disable()
		{
			a[0] = b[0] = 1;
			a[1] = a[2] = a[3] = b[1] = b[2] = b[3] = 0;
		}

		double a[4];
		double b[4];
	};
}