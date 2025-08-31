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

#include "ShelvingFilter.h"
#include <complex>
#include <functional>

using namespace toob;



void ShelvingFilter::SetHighShelf(float db, float fC)
{
    if (db == 0) {
        Disable();
        return;
    }
    if (db == 0) 
    {
        Disable();
        return;
    }

    double Q = 0.7;
    double A = Db2Af(db/2);

    
    // H(s) = A* (s^2 + 1/(sqrt(A)*Q)*s + 1/A)
    //       / (1/A*s^2 + 1/(sqrt(A)*Q)*s + 1)
    
    double invsqrtAQ = 1/(std::sqrt(A)*Q);
    prototype.b[2] = A;
    prototype.b[1] = A*invsqrtAQ;
    prototype.b[0] = 1.0;
    prototype.a[2] = 1.0/A;
    prototype.a[1] = invsqrtAQ;
    prototype.a[0] = 1;

    this->cutoffFrequency = fC;
    ShelvingFilter::SetCutoffFrequency(this->cutoffFrequency);
}

void ShelvingFilter::SetLowShelf(float db, float fC)
{
    if (db == 0) 
    {
        Disable();
        return;
    }

    double Q = 0.7;
    double A = Db2Af(db/2);


    // H(s) = A* (s^2 + sqrt(A)/Q*s + A)
    //       / (A*s^2 + (sqrt(A)/Q*s + 1)
    
    double sqrtAByQ = std::sqrt(A)/Q;
    prototype.b[2] = A;
    prototype.b[1] = A*sqrtAByQ;
    prototype.b[0] = A*A;
    prototype.a[2] = A;
    prototype.a[1] = sqrtAByQ;
    prototype.a[0] = 1;

    this->cutoffFrequency = fC;
    ShelvingFilter::SetCutoffFrequency(this->cutoffFrequency);

}
