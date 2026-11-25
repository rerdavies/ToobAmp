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

#include "PowerOfTwoResampler.hpp"
#include "../TestAssert.hpp"
#include <cmath>
#include <numbers>
#include <vector>
#include <iostream>
#include <cstdlib>

using namespace lsnumerics;

// A constant (DC) input should, once the filter has filled with the
// constant value, produce a constant output of the same value.
static void testDc()
{
    constexpr int RATE = 4;
    constexpr int FILTER_SIZE = 12;
    PowerOfTwoUpsampler<RATE, FILTER_SIZE> upsampler;
    upsampler.Reset();

    const float dcValue = 0.73f;
    float lastOutput = 0;
    for (int i = 0; i < FILTER_SIZE * 2; ++i)
    {
        upsampler.TickInput(dcValue);
        for (int p = 0; p < RATE; ++p)
        {
            lastOutput = upsampler.TickOutput();
        }
    }
    TEST_ASSERT(std::abs(lastOutput - dcValue) < 1E-3);
}

// A low-frequency sine wave should be interpolated without significant
// amplitude error, and without overruning the internal buffers (verified
// by running under AddressSanitizer).
static void testSine()
{
    constexpr int RATE = 4;
    constexpr int FILTER_SIZE = 12;
    PowerOfTwoUpsampler<RATE, FILTER_SIZE> upsampler;
    upsampler.Reset();

    const double frequency = 0.05; // cycles/input sample, well below input Nyquist/RATE.
    const size_t nSamples = 2000;

    std::vector<float> outputs;
    outputs.reserve(nSamples * RATE);
    for (size_t i = 0; i < nSamples; ++i)
    {
        float value = (float)std::sin(2 * std::numbers::pi * frequency * i);
        upsampler.TickInput(value);
        for (int p = 0; p < RATE; ++p)
        {
            outputs.push_back(upsampler.TickOutput());
        }
    }

    // Skip the filter warm-up region, and check that the steady-state
    // amplitude is close to the input amplitude of 1.0.
    double maxAbs = 0;
    for (size_t i = FILTER_SIZE * RATE * 2; i < outputs.size(); ++i)
    {
        maxAbs = std::max(maxAbs, (double)std::abs(outputs[i]));
    }
    TEST_ASSERT(maxAbs > 0.9 && maxAbs < 1.1);
}

// Run enough input/output cycles, with buffer sizes that are not multiples
// of each other, to exercise every wrap-around case of the input and output
// circular buffers. Run under AddressSanitizer to catch out-of-bounds
// buffer access.
static void testWrapAround()
{
    PowerOfTwoUpsampler<4, 7> upsampler47;
    upsampler47.Reset();
    for (int i = 0; i < 10000; ++i)
    {
        upsampler47.TickInput((float)i);
        for (int p = 0; p < 4; ++p)
        {
            upsampler47.TickOutput();
        }
    }

    PowerOfTwoUpsampler<8, 5> upsampler85;
    upsampler85.Reset();
    for (int i = 0; i < 10000; ++i)
    {
        upsampler85.TickInput((float)i);
        for (int p = 0; p < 8; ++p)
        {
            upsampler85.TickOutput();
        }
    }
}

// A constant (DC) input should, once the filter has settled, produce a
// constant decimated output of the same value (Butterworth lowpass has
// unity DC gain).
static void testDownsamplerDc()
{
    constexpr int RATE = 4;
    constexpr int FILTER_SIZE = 12;
    PowerOfTwoDownsampler<RATE, FILTER_SIZE> downsampler;
    downsampler.Reset();

    const float dcValue = -0.42f;
    float lastOutput = 0;
    for (int i = 0; i < FILTER_SIZE * 20; ++i)
    {
        for (int p = 0; p < RATE; ++p)
        {
            downsampler.TickInput(dcValue);
        }
        lastOutput = downsampler.TickOutput();
    }
    TEST_ASSERT(std::abs(lastOutput - dcValue) < 1E-3);
}

// Measures the steady-state decimated output amplitude of a unit-amplitude
// sine wave at the given super-sampled-rate frequency (in cycles/sample).
template <int RATE, int FILTER_SIZE>
static double measureDownsamplerResponse(double frequency)
{
    PowerOfTwoDownsampler<RATE, FILTER_SIZE> downsampler;
    downsampler.Reset();

    const size_t nOutputs = 4000;
    double maxAbs = 0;
    size_t inputIndex = 0;
    for (size_t n = 0; n < nOutputs; ++n)
    {
        for (int p = 0; p < RATE; ++p)
        {
            float value = (float)std::sin(2 * std::numbers::pi * frequency * inputIndex);
            downsampler.TickInput(value);
            ++inputIndex;
        }
        float output = downsampler.TickOutput();
        if (n > nOutputs / 2) // skip the filter's warm-up/settling region.
        {
            maxAbs = std::max(maxAbs, (double)std::abs(output));
        }
    }
    return maxAbs;
}

// A sine wave well below the decimated output's Nyquist frequency should
// pass through with close to unity gain; a sine wave above it (which would
// otherwise alias into the decimated output) must be strongly attenuated by
// the anti-aliasing filter.
static void testDownsamplerAntiAliasing()
{
    constexpr int RATE = 4;
    constexpr int FILTER_SIZE = 12;
    // Output Nyquist, normalized to the super-sampled rate, is 1/(2*RATE) = 0.125.
    double passbandGain = measureDownsamplerResponse<RATE, FILTER_SIZE>(0.02);
    double stopbandGain = measureDownsamplerResponse<RATE, FILTER_SIZE>(0.3);

    TEST_ASSERT(passbandGain > 0.9 && passbandGain < 1.1);
    TEST_ASSERT(stopbandGain < 0.05);
}

// Run the downsampler for many cycles, with both even and odd FILTER_SIZE
// (the latter exercising the single first-order/real-pole section), and
// confirm the filter remains stable (bounded, finite output for bounded
// input).
static void testDownsamplerStability()
{
    PowerOfTwoDownsampler<4, 7> downsampler47;
    downsampler47.Reset();
    for (int i = 0; i < 20000; ++i)
    {
        for (int p = 0; p < 4; ++p)
        {
            downsampler47.TickInput(std::sin(0.3f * i));
        }
        float output = downsampler47.TickOutput();
        TEST_ASSERT(std::isfinite(output) && std::abs(output) < 2.0f);
    }

    PowerOfTwoDownsampler<8, 5> downsampler85;
    downsampler85.Reset();
    for (int i = 0; i < 20000; ++i)
    {
        for (int p = 0; p < 8; ++p)
        {
            downsampler85.TickInput(std::sin(0.3f * i));
        }
        float output = downsampler85.TickOutput();
        TEST_ASSERT(std::isfinite(output) && std::abs(output) < 2.0f);
    }
}

int main(int, char **)
{
    std::cout << "== PowerOfTwoResamplerTest ====" << std::endl;
    try
    {
        testDc();
        testSine();
        testWrapAround();
        testDownsamplerDc();
        testDownsamplerAntiAliasing();
        testDownsamplerStability();
    }
    catch (const std::exception &e)
    {
        std::cout << "PowerOfTwoResamplerTest failed: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
