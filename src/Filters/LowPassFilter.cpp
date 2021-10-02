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