#pragma once
#include "../std.h"
#include <stddef.h>

namespace TwoPlay {
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