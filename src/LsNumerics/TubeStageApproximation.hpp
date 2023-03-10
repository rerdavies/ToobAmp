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
#include "PiecewiseChebyshevApproximation.hpp"
namespace LsNumerics {

class TubeStageApproximation: public PiecewiseChebyshevApproximation {
public:
    TubeStageApproximation();

    double At(double x){
        if (x < minValue)
        {
            double x0 = PiecewiseChebyshevApproximation::At(minValue);
            double dx = PiecewiseChebyshevApproximation::DerivativeAt(minValue);
            return x0 + (x-minValue)*dx;
        }
        if (x > maxValue)
        {
            double x0 = PiecewiseChebyshevApproximation::At(maxValue);
            double dx = PiecewiseChebyshevApproximation::DerivativeAt(maxValue);
            return x0 + dx*(x-maxValue);
        }
        return PiecewiseChebyshevApproximation::At(x);
    }
};

extern TubeStageApproximation gTubeStageApproximation;

} // namespace