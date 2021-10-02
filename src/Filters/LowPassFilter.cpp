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

#include "../std.h"

#include "LowPassFilter.h"

using namespace TwoPlay;


FilterCoefficients2 LowPassFilter::LOWPASS_PROTOTYPE = FilterCoefficients2(
	0.8291449788086549, 0, 0,
	0.8484582463996709, 1.156251050939778,1);


LowPassFilter::LowPassFilter()
: AudioFilter2(LOWPASS_PROTOTYPE)
{

}

LowPassFilter::LowPassFilter(float minFrequency, float maxFrequency, float disabledFrequency)
: AudioFilter2(LOWPASS_PROTOTYPE, minFrequency,maxFrequency,disabledFrequency)
{

}