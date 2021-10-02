#pragma once
#include "AudioFilter.h"


namespace TwoPlay {
    // 6th order Lowpass Elliptic Filter, 1db bandpass ripple, -80db bandstop ripple, 0.2 PI radians/sample (17400 @ 44100)
    // provides -60db filtering at 17400, and -80 db at ~12khz.

    class DownsamplingLowPassFilter: public AudioFilter {
    public:
        DownsamplingLowPassFilter();

    private:


        // frequency not setable.
		int Frequency;
		void Disable() { }
		virtual void SetCutoffFrequency(float frequency) { UNUSED(frequency);}

    };
}