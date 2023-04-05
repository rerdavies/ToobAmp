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
#include "../std.h"
#include <stddef.h>

namespace toob {
	class FilterCoefficients {
	public:
		FilterCoefficients()
		{
			length = 0;
			a = b = 0;
		}
		FilterCoefficients(size_t length)
		{
			this->length = length;
			a = new double[length];
			b = new double[length];
			Disable();
		}
		FilterCoefficients(size_t length, const double*b, const double *a)
		{
			this->length = length;
			this->a = new double[length];
			this->b = new double[length];
			for (size_t i = 0; i < length; ++i)
			{
				this->a[i] = a[i];
				this->b[i] = b[i];
			}
		}
		FilterCoefficients(const FilterCoefficients& other)
		{
			a = b = 0;
			Copy(other);
		}
		void Copy(const FilterCoefficients&other);

		~FilterCoefficients()
		{
			delete[] a;
			delete[] b;
		}

		const FilterCoefficients& operator=(const FilterCoefficients&other)
		{
			Copy(other);
			return *this;
		}
		void Disable()
		{
			for (size_t i = 0; i < length; ++i)
			{
				a[i] = b[i] = 0;
			}
			if (length > 0)
			{
				a[0] = b[0] = 1;
			}
		}

		size_t length;
		double *a = 0;
		double *b = 0;
	};
}