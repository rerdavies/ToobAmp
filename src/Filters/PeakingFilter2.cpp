/*
 *   Copyright (c) 2023 Robin E. R. Davies
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

#include "PeakingFilter2.h"
#include <complex>
#include <functional>

using namespace toob;

void PeakingFilter2::Set(float fc, float gainDb, float q)
{
    // Q_new = Q_base * pow(2, abs(dBgain) / K)


    double g = Db2Af(gainDb);


    //float adjustedQ = q / (1.0f + 0.2f * (g - 1.0f));
    float adjustedQ = q;
    if (gainDb > 0) 
    {
        adjustedQ += gainDb/12;
    }
    else  {
        adjustedQ = adjustedQ/2- gainDb/12;
    }
    double B = 1.0/adjustedQ;
    // H(s) = (s^2+ gBs + 1) / (s^2 + Bs + 1);

    prototype.b[2] = 1;
    prototype.b[1] = g*B;
    prototype.b[0] = 1;
    prototype.a[2] = 1;
    prototype.a[1] = B;
    prototype.a[0] = 1;

    this->cutoffFrequency = fc;
    PeakingFilter2::SetCutoffFrequency(this->cutoffFrequency);
}

