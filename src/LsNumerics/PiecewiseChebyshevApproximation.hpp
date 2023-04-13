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
#include <functional>
#include <vector>
#include "LsChebyshevApproximation.hpp"
#include <exception>
#include <iostream>
#include <iomanip>

namespace LsNumerics {

	using namespace std;
	class PiecewiseChebyshevApproximation {
	private:
		std::function<double(double)> derivative = nullptr;
		std::function<double(double)> function;
		size_t maxIndex;
		int chebyshevOrder;
		bool checkMaxError;
		double valueToIndexSlope;
		double indexToValueSlope;

		std::vector<ChebyshevApproximation> interpolators;
	protected:
		double minValue;
		double maxValue;

	public:
		PiecewiseChebyshevApproximation(
			std::function<double(double)> function, double minValue, double maxValue, size_t segmentCount, int chebyshevOrder = 5, bool checkMaxError = true);


		PiecewiseChebyshevApproximation(double minValue, double maxValue, size_t maxIndex, int chebyshevOrder,
			const std::vector<ChebyshevApproximation>& interpolators);


	private:
		ChebyshevApproximation CreateApproximant(size_t index)
		{
			double minValue, maxValue;
			GetRangeFromIndex(index, &minValue, &maxValue);
			if (!derivative)
			{
				ChebyshevApproximation  result = ChebyshevApproximation(&function, minValue, maxValue, chebyshevOrder);
				if (checkMaxError)
				{
					CalculateError(result, minValue, maxValue);
					CalculateDerivativeError(result, minValue, maxValue);
				}
				return result;
			}
			else {
				ChebyshevApproximation result = ChebyshevApproximation(&function, &derivative, minValue, maxValue, chebyshevOrder);
				if (checkMaxError)
				{
					CalculateError(result, minValue, maxValue);
					CalculateDerivativeError(result, minValue, maxValue);
				}
				return result;
			}

		}
		void GetRangeFromIndex(size_t index, double* minValue, double* maxValue) const
		{
			*minValue = this->minValue + index * indexToValueSlope;
			*maxValue = this->minValue + (index + 1) * indexToValueSlope;

		}
		size_t GetIndexFromValue(double x) const
		{
			return (size_t)
				std::floor((x - minValue) * valueToIndexSlope);

		}
		void CalculateError(const ChebyshevApproximation& result, double minValue, double maxValue);
		void CalculateDerivativeError(const ChebyshevApproximation& result, double minValue, double maxValue);

		double maxError = 0;
		double errorX = NAN;
		double maxDerivativeError = 0;
		double derivativeErrorX = NAN;
	public:
		double GetMaxError()
		{
			if (!checkMaxError) throw std::invalid_argument("constructed with calculateMaxError=false");
			return maxError;
		}
		double GetMaxDerivativeError()
		{
			if (!checkMaxError) throw std::invalid_argument("constructed with calculateMaxError=false");
			return maxDerivativeError;
		}
		double At(double x) const
		{
			if (x < minValue || x > maxValue)
			{
				throw std::invalid_argument("Invalid argument.");
			}
			size_t index = GetIndexFromValue(x);

			return interpolators[index].At(x);
		}
		double DerivativeAt(double x) const
		{
			if (x < minValue || x > maxValue)
			{
				throw std::invalid_argument("Invalid argument.");
			}
			size_t index = GetIndexFromValue(x);
			return interpolators[index].DerivativeAt(x);
		}

		void WriteInitializer(std::ostream& s)
		{
			s << setprecision(16) << "{" << minValue << ", " << maxValue << ", " << maxIndex << ", " << chebyshevOrder << "," << std::endl;
			s << "{" << std::endl;
			for (size_t i = 0; i < this->interpolators.size(); ++i)
			{
				s << "    ";
				interpolators[i].WriteInitializer(s);
				s << "," << endl;
			}
			s << "} }" << std::endl;
		}




	};

}
