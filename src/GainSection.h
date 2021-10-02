#pragma once
#include "InputPort.h"
#include "GainStage.h"
#include "DbDezipper.h"
#include "Filters/LowPassFilter.h"
#include "Filters/AudioFilter2.h"

#include <cmath>

namespace TwoPlay 
{

    class GainSection {
        private:
            static FilterCoefficients2 HIPASS_PROTOTYPE;

            GainStage gain;
            LowPassFilter lpFilter;
            AudioFilter2 hpFilter;
            DbDezipper trimVolume;
            float peak;

        public:
            GainSection();
            bool Enable = true;
            RangedDbInputPort Trim = RangedDbInputPort(-20.0f, 20.0f);
            RangedInputPort Gain = RangedInputPort(0.0f,1.0f);
            RangedInputPort LoCut = RangedInputPort(30.0f, 300.0f);
            RangedInputPort HiCut = RangedInputPort(1000.0f, 11000.0f);
            SteppedInputPort Shape = SteppedInputPort(0,2);

            void SetSampleRate(double rate);
            void Reset();
            void UpdateControls();


            float GetVu() {
                float t = peak;
                peak = 0;
                return t;
            }

            inline float Tick(float value) { 
                value *= trimVolume.Tick();
                float absX = fabs(value);
                if (absX > peak)
                {
                    peak = absX;
                }
                float x = 
                    gain.Tick( 
                        lpFilter.Tick(
                            hpFilter.Tick(
                                value
                            )));
                return Undenormalize(x);
            }
                
    };
}