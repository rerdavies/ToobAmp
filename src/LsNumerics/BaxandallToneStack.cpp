/*
 *   Copyright (c) 2021 Robin E. R. Davies
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


#include "BaxandallToneStack.hpp"
#include "LsMath.hpp"
#include <cmath>

using namespace LsNumerics;
using namespace std;


double BaxandallToneStack::GetDesignFrequencyResponse(double frequency)
{
    std::complex<double> w {0,  2*LsNumerics::Pi*frequency};

    complex<double> num = 0;
    complex<double> denom = 0;

    complex<double> x = 1;
    for (int i = 0; i < 5; ++i)
    {
        num += b[i]*x;
        denom += a[i]*x;
        x *= w;
    }
    return abs(num/denom)*totalGain;


}
static inline double clampRange(double value)
{
    if (value > 1) return 1;
    if (value < 0) return 0;
    return value;
}
void BaxandallToneStack::Design(double bass, double mid, double treble)
{
    midGainFactor =  Db2Af((2*mid-1)*15);
    totalGain = midGainFactor*activeGainFactor;
    bass = clampRange(bass-mid+0.5);
    treble = clampRange(treble-mid+0.5);
    Design(bass,treble);

}

void BaxandallToneStack::Design(double bass, double treble)
{
    bass = LsNumerics::AudioTaper(bass);
    treble = LsNumerics::AudioTaper(treble);
    double Pb = bass;
    double Pb2 = Pb * Pb;
    double Pt = treble;
    double Pt2 = Pt * Pt;
    double PbPt = Pb * Pt;
    // Analog transfer function, from https://ampbooks.com/mobile/dsp/tonestack
    a[0] = 9.34E10;
    a[1] = -2.975E9 * Pb2 + 3.251E9 * Pb + 7.948E8 * Pt + 2.934E8;
    a[2] = 2.344E5 - 7.761E6 * Pb2 + 1.885E7 * PbPt + 8.434E6 * Pb
            +1.593e6*Pt-1.403e6*Pt2-1.714e7*Pb2*Pt;
    a[3] = -33269 * Pb * Pt2 + 5667 * Pb + 37452 * PbPt - 5311 * Pb2 + 335.3 * (Pt - Pt2) 
                - 34433 * Pb2 * Pt + 30250*Pb2*Pt2+ 39.6;
    a[4] = 7.381 * (PbPt + Pb2 * Pt2 - Pb * Pt2 - Pb2 * Pt) + 0.8712 * (Pb - Pb2);

    b[0] = 8.333E10 * Pb + 1.833E9;
    b[1] = 7.083E8 * PbPt - 3.083E8 * Pb2 + 4.794E8 * Pb + 1.558E7 * Pt;
    b[2] = 844320 * Pb - 2.808e6 * Pb2 * Pt + 232280 * Pt + 4.464e6 * PbPt - 754230 * Pb2 - 1.25e6 * Pb * Pt2 - 27500 * Pt2 + 10010;
    b[3] = 220.2 * (Pb - Pb2) + 8310 * PbPt - 7409 * Pb2 * Pt + 100.1 * Pt + 2750 * Pb2 * Pt2 -60.6*Pt2-3294.5*Pb*Pt2;
    b[4] = 2.202 * (PbPt - Pb2 * Pt) + 1.331 * (Pb2 * Pt2 - Pb * Pt2);
    this->SetSTransform(a,b);
}