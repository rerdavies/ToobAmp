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

#include "PiecewiseChebyshevApproximation.hpp"

using namespace LsNumerics;


void PiecewiseChebyshevApproximation::CalculateError(const ChebyshevApproximation& result, double minValue, double maxValue)
{
    const int LOOPS = 100;
    for (int i = 0; i <= LOOPS; ++i)
    {
        double x = minValue + (maxValue - minValue) * i / LOOPS;
        double expected = function(x);
        double actual = result.At(x);
        double error;
        if (std::abs(expected) > 1) {
            error = std::abs((actual - expected) / expected);
        }
        else
        {
            error = std::abs(actual - expected);
        }
        if (error > maxError)
        {
            maxError = error;
            errorX = x;
            if (error > 1E-7)
            {
               // throw std::exception("Chebyshev approximation failed.");
            }
        }
    }
}
void PiecewiseChebyshevApproximation::CalculateDerivativeError(const ChebyshevApproximation& result, double minValue, double maxValue)
{
    if (!derivative) return;

    const int LOOPS = 1000;
    for (int i = 0; i <= LOOPS; ++i)
    {
        double x = minValue + (maxValue - minValue) * i / LOOPS;
        double expected = derivative(x);
        double actual = result.DerivativeAt(x);
        double error;
        if (std::abs(expected) > 1) {
            error = std::abs((actual - expected) / expected);
        }
        else
        {
            error = std::abs(actual - expected);
        }
        if (error > maxDerivativeError)
        {
            maxDerivativeError = error;
            derivativeErrorX = x;
            if (error > 1E-3)
            {
                throw std::invalid_argument("Chebyshev derivative approximation failed.");
            }
        }
    }
}
