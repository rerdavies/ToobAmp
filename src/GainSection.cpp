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



