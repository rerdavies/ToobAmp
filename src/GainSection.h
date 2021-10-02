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