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

#include "ShelvingLowCutFilter2.h"
#include <complex>
#include <functional>

using namespace toob;


static double Solve(const std::function<double (double wd)>& fn,double xMin,double xMax)
{
    while (true)
    {
        if (xMax-xMin < 1E-12)
        {
            return xMin;
        }
        double xMid = (xMin+xMax)/2;
        double yMid = fn(xMid);
        if (yMid < 0)
        {
            xMin = xMid;
        } else {
            xMax = xMid;
        }
    }
}

void ShelvingLowCutFilter2::Design(float lowDb, float highDb, float fL)
{
    double lowA = LsNumerics::Db2Af(lowDb);
    double highA = LsNumerics::Db2Af(highDb);
    double wC = 1;

    bool lowShelf = lowDb > highDb;
    if (lowShelf)
    {
        double gain = lowA / highA - 1;
        wC = Solve([gain](double wc) {
            double response = std::abs( (std::complex<double>(0,1) + wc*(gain+1))/(std::complex<double>(0,1) + wc));
            return response-gain/2;
            },
            0,1
        );
        // H(s) = (s+ wC(gain+1)) /  (s + wC)
        prototype.b[2] = 0;
        prototype.b[1] = 1;
        prototype.b[0] = wC * (gain + 1);
        prototype.a[2] = 0;
        prototype.a[1] = 1;
        prototype.a[0] = wC;

        prototype.b[0] *= highA;
        prototype.b[1] *= highA;
        prototype.b[2] *= highA;

        ShelvingLowCutFilter2::SetCutoffFrequency(this->cutoffFrequency);

    }
    else
    {
        // H(s) = (1+gain)*(s+wC/(1+gain))/(s+wC)
        double gain = highA / lowA - 1;

        wC = Solve([gain](double wc) {
            double response = std::abs( (std::complex<double>(0,1) + wc*(gain+1))/(std::complex<double>(0,1) + wc));
            return response-gain/2;
            },
            0,1
        );


        prototype.b[2] = 0;
        prototype.b[0] = 1;
        prototype.b[1] = wC * (gain + 1);
        prototype.a[2] = 0;
        prototype.a[0] = 1;
        prototype.a[1] = wC;


        prototype.b[0] *= lowA;
        prototype.b[1] *= lowA;
        prototype.b[2] *= lowA;
        

        ShelvingLowCutFilter2::SetCutoffFrequency(this->cutoffFrequency);
    }

    this->cutoffFrequency = fL;
    AudioFilter2::SetCutoffFrequency(fL);
}

void ShelvingLowCutFilter2::SetLowCutDb(float db)
{
    this->lowCutDb = db;
    if (db > 0)
        db = -db;
    if (db != 0)
    {
        disabled = false;
        float g = Db2Af(db);

        prototype.b[0] = g;
        prototype.b[1] = std::sqrt(g / 2);
        prototype.b[2] = 1;
        prototype.a[0] = prototype.a[2] = 1;
        prototype.a[1] = std::sqrt(2);
        ShelvingLowCutFilter2::SetCutoffFrequency(this->cutoffFrequency);
        
    }
    else
    {
        disabled = true;
        this->zTransformCoefficients.Disable();
    }
}
