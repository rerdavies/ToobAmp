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
#include "AudioFilter2.h"
#include "../LsNumerics/LsMath.hpp"
#include <cmath>
#include <functional>

namespace toob {
    using namespace LsNumerics;

    class ShelvingLowCutFilter2: public AudioFilter2 {
    private:

        float lowCutDb;
        bool disabled;
        float sampleRate;
        float cutoffFrequency = 4000;
    public:
        ShelvingLowCutFilter2()
        {
            SetLowCutDb(0);
        }
        void Design(float lowDb, float highDb, float fC);
        void SetLowCutDb(float db);

        void SetSampleRate(float sampleRate)
        {
            AudioFilter2::SetSampleRate(sampleRate);
            this->sampleRate = sampleRate;
        }

        virtual void SetCutoffFrequency(float frequency)
        {
            this->cutoffFrequency = frequency;
            if (!disabled)
            {
                AudioFilter2::SetCutoffFrequency(frequency);
            }
        }

    };

}