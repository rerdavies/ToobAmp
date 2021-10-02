#pragma once

#include "Filters/LowPassFilter.h"
#include "Filters/DownsamplingLowPassFilter.h"

#define OLD_GAIN_FILTER 0

namespace TwoPlay {
    class GainStage {
    public:
        enum class EShape {
            ATAN = 0,
            TUBE_CLASSA,
            TUBE_CLASSB
        };
    private:
        LowPassFilter upsamplingFilter;
        #if OLD_GAIN_FILTER
            LowPassFilter downsamplingFilter;
        #else
            DownsamplingLowPassFilter downsamplingFilter;
        #endif
        double lastGainIn;

        double tubeMIn,tubeCIn,tubeMOut,tubeCOut;

        double gain;
        double gainScale;

        double GainFn(double value);

        EShape shape = EShape::ATAN;

        void SetClassAGain(float value);
        void SetClassBGain(float value);
    public:
        void SetShape(EShape shape);

        void Reset() 
        {
            upsamplingFilter.Reset();
            downsamplingFilter.Reset();
        }
        void SetGain(float value);

        void SetSampleRate(double rate);
        float Tick(float value);
    };
}