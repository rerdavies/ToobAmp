/*
 *   Copyright (c) 2025 Robin E. R. Davies
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

 /*
    Models an MXR Phase 90 pedal.

    Instead of using four phaser units, the emulation uses a pair of two-phaser units. Each phaser-pair generates one of the 
    two notches that a 4-phaser unit would have. This approach allows precies mapping of the notch 
    frequencies from the LFO-generated frequency.

 */
#pragma once

#include <vector>
#include <cmath>

namespace toob
{
    class AllPassFilter
    {
    private:
        float a1 = 0.0f;
        float zm1 = 0.0f;

    public:
        AllPassFilter() {}

        void setCoefficient(float coefficient)
        {
            a1 = coefficient;
        }

        float process(float input)
        {
            float output = a1 * input + zm1;
            zm1 = input - a1 * output;
            return output;
        }

        void reset()
        {
            zm1 = 0.0f;
        }
    };

    /**
     * @brief A two-unit phaser.
     * 
     * NotchFilter generates a signgle notched filter. A pair of NotchFilters generates 
     * each of the two notches that a four-unit phaser has. This approach allows
     * easier mapping of the LFO notch frequencies from S to Z space.
     * 
     */
    class NotchFilter
    {
    private:
        float a1 = 0.0f;
        float zm1_a = 0.0f;
        float zm1_b = 0.0f;
        float frequencyMultiplier = 1.0f;
        float frequencyMultiplierFactor = 0.0f;
        float sampleRate = 44100;

    public:
        NotchFilter() {}

        void setFrequencyMultiplier(float sampleRate,float frequencyMultiplier) {
            this->sampleRate = sampleRate;
            this->frequencyMultiplier = frequencyMultiplier;
            this->frequencyMultiplierFactor = M_PI / sampleRate *frequencyMultiplier;
        }
        float getFrequencyMultiplier() const {
            return this->frequencyMultiplier;
        }
        void setNotchFrequency(float freq)
        {
            // Convert frequency to filter coefficient
            // The formula for coefficient:
            // a1 = (tan(pi*f/fs) - 1) / (tan(pi*f/fs) + 1)
            float tanValue = tanf(freq * frequencyMultiplierFactor); // M_PI * freq *frequencyMultiplier / sampleRate);
            this->a1 =  (tanValue-1.0) / (tanValue + 1.0f); // coefficient = (tanValue-1.0) / (tanValue + 1.0f);
        }

        float process(float input)
        {
            float output1 = a1 * input + zm1_a;
            zm1_a = input - a1 * output1;

            float output2 = a1 * output1 + zm1_b;
            zm1_b = output1 - a1 * output2;

            return 0.5f*(input + output2);
        }

        void reset()
        {
            zm1_a = 0.0f;
            zm1_b = 0.0f;
        }
    private:
        static inline float QuickTan(float x)
        {
        static const float pisqby4 = 2.4674011002723397f;
        static const float adjpisqby4 = 2.471688400562703f;
        static const float adj1minus8bypisq = 0.189759681063053f;
        float xsq = x * x;
        return x * (adjpisqby4 - adj1minus8bypisq * xsq) / (pisqby4 - xsq);
        }        

    };

    class Phase90Lfo {
    public:
        // characterize the curve and x = 0, x = 0.5, and x = 1, respectively.
        static constexpr float VLO = 180; // y(0) value
        static constexpr float VMID = 260; // y(0.5) value
        static constexpr float VHI = 514; // y(1.0) value.

        // parameters for y(x) = a + 1/(m*x+c) for constraints given above (solved by Grok).

        static constexpr float k = (VHI-VLO)/(VMID-VLO);
        static constexpr float a = VLO - (VHI-VLO)/(k-2);
        static constexpr float m = -(k-2)*(k-2)/((VHI-VLO)*(k-1));
        static constexpr float c = (k-2)/(VHI-VLO);
    private:
        float sampleRate;
        float phase = 0; // range (0..2)
        float dPhase = 0;
    public:
        Phase90Lfo(float sampleRate)
        : sampleRate(sampleRate) {
        }
        float lfoToFreq(float x) {
            return a + 1/(m*x + c);
        }
        void setRate(float hz) {
            this->dPhase = 1/sampleRate*hz;
        }
        void testSetLfoPosition(float value) {
            this->phase = value;
            this->dPhase = 0;
        }
        void reset() {
            phase = 0;
        }

        float tick() {
            // sawtooth, [0..1]
            double x = phase*2;

            if (x > 1.0f) { x = 2.0-x; };
        
            phase += dPhase;
            if (phase >= 1.0) { phase -= 1.0; }

            return lfoToFreq(x);

        }
    };

    class Phaser
    {
    private:
        std::vector<NotchFilter> notchFilters;
        Phase90Lfo lfo;
        float sampleRate;
        float feedback = 0;
        float minFreq, maxFreq;
        float wetDry; // 0 = dry, 1 = wet
    public:
        Phaser(float sampleRate = 4800.0f)
            : sampleRate(sampleRate),
              lfo(sampleRate)
        {
            // Initialize filters
            notchFilters.resize(2);

            // Default settings
            setLfoRate(0.5f);     

            // set relative frequencies of notch filters.
            float fMultiplierLow = tan(M_PI/8.0);
            float fMultiplierHigh = tan(M_PI*3.0/8.0);
            notchFilters[0].setFrequencyMultiplier(sampleRate,1);
            notchFilters[1].setFrequencyMultiplier(sampleRate,fMultiplierHigh/fMultiplierLow);

            reset();
        }
        float getSampleRate() const { return this->sampleRate; }

        void setLfoRate(float rate)
        {
            constexpr double MIN_F = 0.1;
            constexpr double MAX_F = 3.7;

            float hz = MIN_F*pow(MAX_F/MIN_F,rate);
            lfo.setRate(hz);
        }
        void setFeedback(float feedback) {
            this->feedback = feedback*0.3;
        }
        // test ony, set fixed LFO position.
        void testSetLfoPosition(float value) {
            lfo.testSetLfoPosition(value);
        }

        float process(float inputSample)
        {
            float freq = lfo.tick();

            for (size_t i = 0; i < this->notchFilters.size(); ++i)
            {
                notchFilters[i].setNotchFrequency(freq);
            }

            // Apply feedback and process through all stages
            float input = inputSample;
            float output = input;

            for (size_t i = 0; i < notchFilters.size(); ++i)
            {
                output = notchFilters[i].process(output);
            }

            return output;
        }

        // Reset all internal states
        void reset()
        {
            lfo.reset();
            for (size_t i = 0; i < notchFilters.size(); ++i)
            {
                notchFilters[i].reset();
            }
        }
    };
} // namespace