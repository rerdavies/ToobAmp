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

#include "GainSection.h"

using namespace toob;

// Chebyshev HP I, 0.2db ripple, -3db at 1
FilterCoefficients2 GainSection::HIPASS_PROTOTYPE = FilterCoefficients2(
    0, 0, 0.982613364180136,
    1.102510328053848, 1.097734328563927, 1);

void GainSection::SetSampleRate(double rate)
{
    hpFilter.SetSampleRate(rate);
    lpFilter.SetSampleRate(rate);
    this->trimVolume.SetSampleRate(rate);
    gain.SetSampleRate(rate);
}

GainSection::GainSection()
    : hpFilter(GainSection::HIPASS_PROTOTYPE)
{
}

void GainSection::Reset()
{
    gain.Reset();
    hpFilter.Reset();
    lpFilter.Reset();
    peakMax = 0;
    peakMin = 0;
}

void GainSection::UpdateControls(
)
{
    if (LoCut.HasChanged())
    {
        float f = LoCut.GetValue();
        if (f == LoCut.GetMinValue())
        {
            hpFilter.Disable();
        }
        else
        {
            hpFilter.SetCutoffFrequency(f);
        }
    }
    if (HiCut.HasChanged())
    {
        float f = HiCut.GetValue();
        if (f == HiCut.GetMaxValue())
        {
            lpFilter.Disable();
        }
        else
        {
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
    if (Bias.HasChanged())
    {
        gain.SetBias(Bias.GetValue());
    }
}

void GainSection::WriteShapeCurve(
    LV2_Atom_Forge *forge,
    LV2_URID propertyUrid)
{
    const int NSAMPLES = 100;
    float data[NSAMPLES];

    for (int i = 0; i < NSAMPLES; ++i)
    {
        double x = i * 2.0 / (NSAMPLES - 1) - 1.0;
        data[i] = this->gain.GainFn(x);
    }

    lv2_atom_forge_frame_time(forge, 0);

    LV2_Atom_Forge_Frame objectFrame;
    
    lv2_atom_forge_object(forge, &objectFrame, 0, gainStageUris.patch__Set);

    lv2_atom_forge_key(forge, gainStageUris.patch__property);
    lv2_atom_forge_urid(forge, propertyUrid);
    lv2_atom_forge_key(forge, gainStageUris.patch__value);

    LV2_Atom_Forge_Frame vectorFrame;
    lv2_atom_forge_vector_head(forge, &vectorFrame, sizeof(float), gainStageUris.atom__float);
    for (int i = 0; i < NSAMPLES; ++i)
    {
        lv2_atom_forge_float(forge, data[i]);
    }
    lv2_atom_forge_pop(forge, &vectorFrame);

    lv2_atom_forge_pop(forge, &objectFrame);
}
