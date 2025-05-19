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

#include <cmath>
#include <cstdint>
#include <array>

#pragma once
namespace toob::graphic_eq
{
    class PeakingFilter
    {
    public:
        PeakingFilter() : b0(0.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f),
                          x1(0.0f), x2(0.0f), y1(0.0f), y2(0.0f) {}

        void updateCoefficients(float gain, float fc, double q, double fs)
        {
            // ref: https://www.dsprelated.com/freebooks/filters/Peaking_Equalizers.html.  2025-5-17


            Q = q; // fs / bw;
            float wcT = 2 * M_PI * fc / fs;

            K = tanf(wcT / 2);
            updateGain(gain);
        }
        void updateGain(float gain)
        {
            float V = gain;
            float K2 = K*K;

            float b0 = 1 + V * K / Q + K2;
            float b1 = 2 * (K2 - 1);
            float b2 = 1 - V * K / Q + K2;
            float a0 = 1 + K / Q + K2;
            float a1 = 2 * (K2 - 1);
            float a2 = 1 - K / Q + K2;

            float norm = 1/a0;
            this->a1 = a1*norm;
            this->a2 = a2*norm;
            this->b0 = b0*norm;
            this->b1 = b1*norm;
            this->b2 = b2*norm;

        }
        void reset() {
            x1 = 0; x2 = 0; y1 = 0; y2 = 0;
        }

        float tick(float input)
        {
            float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = input;
            y2 = y1;
            y1 = output;
            return output;
        }

    private:
        float Q, K;
        float b0, b1, b2, a1, a2;
        float x1, x2, y1, y2; // Per-channel states
    };

    class GraphicEQ
    {
    public:
        GraphicEQ(double rate) : sampleRate(rate)
        {
            inputL = nullptr;
            inputR = nullptr;
            outputL = nullptr;
            outputR = nullptr;
            for (int i = 0; i < 7; ++i)
            {
                gainValues[i] = 1.0f; // 0 dB
            }

            // Initialize filter coefficients
            float freq = 100;
            for (int band = 0; band < 7; ++band)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    filters[band][ch].updateCoefficients(gainValues[ch],freq,q, sampleRate);
                }
                freq *= 2;
            }
        }

        void setGain(uint32_t band, float value)
        {
            gainValues[band] = value;
            for (int32_t ch = 0; ch < 2; ++ch) 
            {
                this->filters[band][ch].updateGain(value);
            }
        }

        void reset() {
            for (int band = 0; band < 7; ++band)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    filters[band][ch].reset();
                }
            }
        }
        void updateGains() {
            for (int band = 0; band < 7; ++band)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    filters[band][ch].updateGain(gainValues[ch]);
                }
            }
        }
        void setLevel(float value)
        {
            this->level = value;
        }
        void run(uint32_t n_samples)
        {
            for (uint32_t i = 0; i < n_samples; ++i)
            {
                float left = inputL[i];
                float right = inputR[i];

                // Process each band
                for (int band = 0; band < 7; ++band)
                {
                    // Left channel (channel 0)
                    left = filters[band][0].tick(left);

                    // Right channel (channel 1)
                    right = filters[band][1].tick(right);
                }

                outputL[i] = left * level;
                outputR[i] = right * level;
            }
        }

        void setIO(const float *inputL, const float *inputR, float *outputL, float *outputR)
        {
            this->inputL = inputL;
            this->inputR = inputR;
            this->outputL = outputL;
            this->outputR = outputR;
        }

    private:
        const float *inputL;
        const float *inputR;
        float *outputL;
        float *outputR;
        float level = 1.0f;
        std::array<float, 7> gainValues;
        std::array<std::array<PeakingFilter, 2>, 7> filters; // [band][channel]
        double sampleRate;
        static constexpr float q = 2.871f/2.05f; // 1 octave.
    };

} // namespace
