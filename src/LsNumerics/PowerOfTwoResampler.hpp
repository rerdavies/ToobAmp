/*
 *   Copyright (c) 2026 Robin E. R. Davies
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



#include <array>
#include <cmath>
#include "../restrict.hpp"
#include "LsMath.hpp"

namespace lsnumerics {

    template<int UPSAMPLING_RATE=4, int FILTER_SIZE=12> 
    class PowerOfTwoUpsampler {
    public:
        PowerOfTwoUpsampler();

        void Reset();
        void TickInput(float value);
        float TickOutput();

    private:
        size_t inputOffset = 0;
        size_t outputOffset = 0;
        size_t outputPhase = 0;

        std::array<float,FILTER_SIZE*2> inputBuffer;
        std::array<std::array<float,FILTER_SIZE>,UPSAMPLING_RATE> phaseFilters;
    };

    // Decimates by DOWNSAMPLING_RATE, using a FILTER_SIZE'th-order Butterworth
    // IIR low-pass filter (cascaded biquads) to band-limit the super-sampled
    // input below the decimated output's Nyquist frequency, so that TickOutput()
    // does not introduce aliasing.
    template<int DOWNSAMPLING_RATE=4, int FILTER_SIZE=12> 
    class PowerOfTwoDownsampler {
    public:
        PowerOfTwoDownsampler();

        void Reset();
        void TickInput(float value);
        float TickOutput();

    private:
        // A single second-order (biquad) IIR section, in Direct-Form-II-
        // Transposed form (z1,z2 are the section's state variables).
        struct BiquadSection {
            double b0 = 1, b1 = 0, b2 = 0;
            double a1 = 0, a2 = 0;
            double z1 = 0, z2 = 0;
        };
        // One section per conjugate pole pair, plus one (first-order) section
        // for the single real pole when FILTER_SIZE is odd.
        static constexpr size_t N_SECTIONS = (FILTER_SIZE + 1) / 2;

        std::array<BiquadSection, N_SECTIONS> sections;
        float lastOutput = 0;
    };

    //////////////

    template<int UPSAMPLING_RATE, int FILTER_SIZE> 
    PowerOfTwoUpsampler<UPSAMPLING_RATE, FILTER_SIZE>::PowerOfTwoUpsampler()
    {
        // Initialize the polyphase upsampling filters.
        //
        // Design a windowed-sinc low-pass prototype filter h[0..N-1] of length
        // N = FILTER_SIZE*UPSAMPLING_RATE, with a cutoff at the input Nyquist
        // frequency (i.e. 1/UPSAMPLING_RATE of the output Nyquist frequency),
        // and decompose it into UPSAMPLING_RATE polyphase sub-filters of
        // length FILTER_SIZE, one per output phase.
        constexpr size_t N = (size_t)FILTER_SIZE * (size_t)UPSAMPLING_RATE;
        constexpr double cutoff = 1.0 / UPSAMPLING_RATE; // normalized to output Nyquist (= 1.0)
        constexpr double center = (N - 1) / 2.0;

        std::array<double, N> prototype;
        for (size_t n = 0; n < N; ++n) 
        {
            double x = n - center;
            double sinc = (x == 0.0) 
                ? cutoff 
                : std::sin(LsNumerics::Pi * cutoff * x) / (LsNumerics::Pi * x);
            // Blackman window, for good stop-band attenuation.
            double window = 0.42
                - 0.5 * std::cos(2 * LsNumerics::Pi * n / (N - 1))
                + 0.08 * std::cos(4 * LsNumerics::Pi * n / (N - 1));
            prototype[n] = sinc * window;
        }

        // Normalize for unity DC passband gain, and scale by UPSAMPLING_RATE
        // to compensate for the amplitude loss caused by the zero-stuffed
        // samples that this filter is implicitly interpolating.
        double sum = 0;
        for (double v : prototype) sum += v;
        double gain = UPSAMPLING_RATE / sum;

        // Decompose into polyphase sub-filters. phaseFilters[p][i] multiplies
        // the i'th-oldest of the last FILTER_SIZE input values (i == FILTER_SIZE-1 
        // being the most recent), to produce the output sample at fractional 
        // position p/UPSAMPLING_RATE past that most recent input value.
        for (size_t p = 0; p < (size_t)UPSAMPLING_RATE; ++p)
        {
            for (size_t i = 0; i < (size_t)FILTER_SIZE; ++i)
            {
                size_t k = FILTER_SIZE - 1 - i;
                phaseFilters[p][i] = (float)(prototype[p + k * UPSAMPLING_RATE] * gain);
            }
        }
        Reset();
    }
    template<int UPSAMPLING_RATE, int FILTER_SIZE> 
    inline void PowerOfTwoUpsampler<UPSAMPLING_RATE, FILTER_SIZE>::Reset() {
        inputOffset = 0;
        outputOffset = 0;
        outputPhase = 0;
        for (size_t i = 0; i < inputBuffer.size(); ++i) 
        {
            inputBuffer[i] = 0;
        }
    }
    template<int UPSAMPLING_RATE, int FILTER_SIZE> 
    inline void PowerOfTwoUpsampler<UPSAMPLING_RATE, FILTER_SIZE>::TickInput(float value) {
        // put the sample into the array twice, so that we can always get a pointer to the last FILTER_SIZE 
        // input values, in consecutive memory locations.
        inputBuffer[inputOffset] = value;
        inputBuffer[inputOffset+FILTER_SIZE] = value;
        ++inputOffset;
        if (inputOffset >= FILTER_SIZE) inputOffset = 0;
    }
    template<int UPSAMPLING_RATE, int FILTER_SIZE> 
    inline float PowerOfTwoUpsampler<UPSAMPLING_RATE, FILTER_SIZE>::TickOutput() {
        float* restrict pInput = &(inputBuffer.at(outputOffset));
        float* restrict phaseFilter = phaseFilters[outputPhase].data();

        double sum = 0;
        for (size_t i = 0; i < FILTER_SIZE; ++i) 
        {
            sum += pInput[i] * phaseFilter[i];
        }
        if (++outputPhase >= UPSAMPLING_RATE) 
        {
            outputPhase = 0;
            ++outputOffset;
            if (outputOffset >= FILTER_SIZE)
            {
                outputOffset = 0;
            }
        }
        return (float)sum;
    }

    template<int DOWNSAMPLING_RATE, int FILTER_SIZE> 
    PowerOfTwoDownsampler<DOWNSAMPLING_RATE, FILTER_SIZE>::PowerOfTwoDownsampler()
    {
        // Design a FILTER_SIZE'th-order Butterworth low-pass filter, with cutoff
        // at the output (decimated) Nyquist frequency -- i.e. 1/DOWNSAMPLING_RATE 
        // of the input (super-sampled) Nyquist frequency -- and realize it as a 
        // cascade of Direct-Form-II-Transposed biquad sections (plus a trailing
        // first-order section if FILTER_SIZE is odd).
        //
        // Analog Butterworth poles lie on a circle of radius wc, at angles
        // theta_k = Pi/2 + (2k-1)*Pi/(2*FILTER_SIZE), k = 1..FILTER_SIZE, and are 
        // mapped to the digital domain with the bilinear transform z = (2+p)/(2-p), 
        // using the pre-warped analog cutoff wc = 2*tan(wd/2).
        constexpr int N = FILTER_SIZE;
        constexpr double wd = LsNumerics::Pi / DOWNSAMPLING_RATE; // digital cutoff, radians/sample.
        const double wc = 2.0 * std::tan(wd / 2.0); // pre-warped analog cutoff.

        size_t sectionIndex = 0;
        double rawGain = 1.0; // DC gain of the (not yet normalized) cascade.

        for (int k = 1; k <= N / 2; ++k)
        {
            double theta = LsNumerics::Pi * (2 * k - 1) / (2.0 * N) + LsNumerics::Pi / 2.0;
            double pRe = wc * std::cos(theta);
            double pIm = wc * std::sin(theta);

            // Bilinear transform of one pole of the conjugate pair: z = (2+p)/(2-p).
            double denomRe = 2.0 - pRe;
            double denomIm = -pIm;
            double denomMagSq = denomRe * denomRe + denomIm * denomIm;
            double numRe = 2.0 + pRe;
            double numIm = pIm;
            double zRe = (numRe * denomRe + numIm * denomIm) / denomMagSq;
            double zIm = (numIm * denomRe - numRe * denomIm) / denomMagSq;

            // Biquad built from the conjugate pole pair (z, z*):
            //   denominator = 1 - 2*Re(z)*z^-1 + |z|^2*z^-2
            //   numerator   = (1+z^-1)^2 = 1 + 2*z^-1 + z^-2  (double zero at Nyquist)
            BiquadSection &section = sections[sectionIndex++];
            section.a1 = -2.0 * zRe;
            section.a2 = zRe * zRe + zIm * zIm;
            section.b0 = 1.0;
            section.b1 = 2.0;
            section.b2 = 1.0;

            rawGain *= 4.0 / (1.0 + section.a1 + section.a2);
        }
        if (N % 2 == 1)
        {
            // One real pole remains, at theta = Pi (i.e. p = -wc).
            double zRe = (2.0 - wc) / (2.0 + wc);

            BiquadSection &section = sections[sectionIndex++];
            section.a1 = -zRe;
            section.a2 = 0.0;
            section.b0 = 1.0;
            section.b1 = 1.0;
            section.b2 = 0.0;

            rawGain *= 2.0 / (1.0 + section.a1);
        }

        // Distribute the compensating gain (so that the cascade's DC gain is
        // exactly 1.0) evenly across all sections' numerator coefficients.
        double perSectionGain = std::pow(1.0 / rawGain, 1.0 / sections.size());
        for (auto &section : sections)
        {
            section.b0 *= perSectionGain;
            section.b1 *= perSectionGain;
            section.b2 *= perSectionGain;
        }
    }
    template<int DOWNSAMPLING_RATE, int FILTER_SIZE> 
    inline void PowerOfTwoDownsampler<DOWNSAMPLING_RATE, FILTER_SIZE>::Reset() {
        for (auto &section : sections)
        {
            section.z1 = 0;
            section.z2 = 0;
        }
        lastOutput = 0;
    }
    template<int DOWNSAMPLING_RATE, int FILTER_SIZE> 
    inline void PowerOfTwoDownsampler<DOWNSAMPLING_RATE, FILTER_SIZE>::TickInput(float value) {
        // Filter every super-sampled input through the IIR cascade, so that
        // the signal is properly band-limited by the time TickOutput() picks
        // off a decimated sample.
        double v = value;
        for (auto &section : sections)
        {
            double y = section.b0 * v + section.z1;
            section.z1 = section.b1 * v - section.a1 * y + section.z2;
            section.z2 = section.b2 * v - section.a2 * y;
            v = y;
        }
        lastOutput = (float)v;
    }
    template<int DOWNSAMPLING_RATE, int FILTER_SIZE> 
    inline float PowerOfTwoDownsampler<DOWNSAMPLING_RATE, FILTER_SIZE>::TickOutput() {
        // The anti-aliasing filter has already run once per input sample in
        // TickInput(); decimation is simply picking off the latest filtered value.
        return lastOutput;
    }

} // namespace