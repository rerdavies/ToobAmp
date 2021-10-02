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

#include "GainSection.h"


using namespace TwoPlay;


// Chebyshev HP I, 0.2db ripple, -3db at 1
FilterCoefficients2  GainSection::HIPASS_PROTOTYPE = FilterCoefficients2(
	0, 0, 0.982613364180136,
	1.102510328053848,1.097734328563927,1);

void GainSection::SetSampleRate(double rate) {
    hpFilter.SetSampleRate(rate);
    lpFilter.SetSampleRate(rate);
    this->trimVolume.SetSampleRate(rate);
    gain.SetSampleRate(rate);

}

GainSection::GainSection()
:hpFilter(GainSection::HIPASS_PROTOTYPE)
{

}


void  GainSection::Reset() {
    gain.Reset();
    hpFilter.Reset();
    lpFilter.Reset();
    peak = 0;
}


void GainSection::UpdateControls()
{
    if (LoCut.HasChanged())
    {
        float f = LoCut.GetValue();
        if (f == LoCut.GetMinValue())
        {
            hpFilter.Disable();
        } else {
            hpFilter.SetCutoffFrequency(f);
        }
    }
    if (HiCut.HasChanged())
    {
        float f = HiCut.GetValue();
        if (f == HiCut.GetMaxValue())
        {
            lpFilter.Disable();
        } else {
            lpFilter.SetCutoffFrequency(f);
        }
    }
    if (Gain.HasChanged())
    {
        gain.SetGain(Gain.GetValue());
    }
    if (Trim.HasChanged())
    {
        trimVolume.SetTarget(Trim.GetDb());
    }
    if (Shape.HasChanged())
    {
        gain.SetShape((GainStage::EShape)(int)(Shape.GetValue()));
    }
}



