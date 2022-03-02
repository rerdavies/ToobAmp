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

#pragma once

#include "std.h"
#include <cmath>

namespace TwoPlay {
    class NoiseGate {
    public:
        enum class EState {
            Disabled,
            Released,
            Releasing,
            Attacking,
            Holding,
        };

    private: 
        bool enabled = true;
        double sampleRate = 0;
        double attackRate = 0;
        double releaseRate = 0;
        int32_t holdSampleDelay = 0;
        double afAttackThreshold = 0;
        double afReleaseThreshold = 0;
        
        EState state = EState::Released;
        double x = 0;
        double dx = 0;
        int32_t holdCount;

    public:
        void Reset() {
            x = 0; dx = 0; 
            holdCount = 0;
            state = enabled?  EState::Released: EState::Disabled;

        }
        EState GetState() { return state; }

        void SetSampleRate(double sampleRate);
        void SetGateThreshold(float decibels);
        void SetEnabled(bool enabled)
        {
            this->enabled = enabled;
            Reset();
        }
        
        inline float Tick(float value)
        {
            if (state == EState::Disabled)
            {
                return value;
            }
            float absValue = std::abs(value);
            if (absValue > afAttackThreshold && state < EState::Attacking)
            {
                state = EState::Attacking;
                dx = attackRate;
                holdCount = this->holdSampleDelay;
            } else if (absValue > afReleaseThreshold && state >= EState::Attacking)
            {
                holdCount = this->holdSampleDelay;
            }
            if (holdCount != 0 && --holdCount == 0)
            {
                state = EState::Releasing;
                dx = -releaseRate;
            }
            x += dx;
            if (x > 1.0)
            {
                x = 1.0;
                dx = 0;
                state = EState::Holding;
            } else if (x < 0)
            {
                x = 0;
                dx = 0;
                state = EState::Released;
            }
            return value*x;
        }

    private :
        int32_t SecondsToSamples(double seconds);
        double SecondsToRate(double seconds);
    };
}