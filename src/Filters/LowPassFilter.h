#pragma once

#include "AudioFilter2.h"


namespace TwoPlay {
    class  LowPassFilter: public AudioFilter2 {
    private:
        static FilterCoefficients2 LOWPASS_PROTOTYPE;

    public:
        LowPassFilter();
        LowPassFilter(float minFrequency, float maxFrequency, float disabledFrequency = -1);

    };
}