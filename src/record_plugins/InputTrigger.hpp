// Copyright (c) 2025 Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <cstddef>
#include <cstdint>

namespace toob {
    class InputTrigger {
    public:
        InputTrigger();
        virtual ~InputTrigger() {}
        void Init(double sampleRate);

        void ThresholdDb(float value);
        float ThresholdD() const { return thresholdDb; }

        void Run(const float *inL, const float *inR, size_t n_samples);
        void Run(const float *inL, size_t n_samples)
        {
            Run(inL, nullptr, n_samples);
        }

        bool Triggered() const { return triggered; }
        size_t TriggerFrame() const { return triggerFrame; }
        bool TriggerLed() const {return triggerLedCount != 0; }
        float ThresholdAfSquared() const { return thresholdAfSquared; }
    private:
        float thresholdDb = 0.0f;
        float thresholdAfSquared = 1.0f;
        size_t triggerSamples = 100;
        size_t triggerLedCount = 0;
        bool triggered = false;
        size_t triggerFrame = 0;
    };
}
