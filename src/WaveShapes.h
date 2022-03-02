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
#include <cmath>

namespace TwoPlay {


// Atan approximating valid in range -1 to 1.
inline double AtanApprox(double x)
{
    double x2 = x*x;
    // to-do: full precision co-efficients!
    return ((((((((0.00286623*x2-0.0161657)
                *x2+0.0429096)
                *x2-0.0752896)
                *x2+0.106563)
                *x2-0.142089)
                *x2+0.199936)
                *x2-0.333331)
                *x2+1)*x;

};

inline double Atan(double value)
{
    if (value > 1)
    {
        return (M_PI/2)- AtanApprox(1/value);
    } else if (value < -1)
    {
        return (-M_PI/2)-AtanApprox(1/value);
    }
    else {
        return AtanApprox(value);
    }
}
inline double AsymmetricAtan(double value)
{
    return Atan(value-0.5);
}


} // namespace