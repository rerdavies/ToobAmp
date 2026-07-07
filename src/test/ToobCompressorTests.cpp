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

#include "catch_include.hpp"
#include <iostream>
#include "../OtaCompressor.hpp"
#include <limits>
#include <numbers>
#include <functional>
#include <fstream>

#include "../ClaudeEmphasisFilter.hpp"

using namespace ota_compressor;

static constexpr float DEFAULT_SAMPLE_RATE = 48000.0f*4;
static std::vector<float> MakeSinWave(float sampleRate, float seconds, float f, float mag)
{
    std::vector<float> result;
    result.resize((size_t)(sampleRate * seconds));

    float x = 0;
    float dx = f / sampleRate * 2 * std::numbers::pi;

    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = std::sin(x) * mag;
        x += dx;
    }
    return result;
}

static float VectorMin(const std::vector<float> &values, size_t start, size_t end)
{
    float minValue = std::numeric_limits<float>::max();
    for (size_t i = start; i < end; ++i)
    {
        float v = values[i];
        if (v < minValue)
        {
            minValue = v;
        }
    }
    return minValue;
}
static float VectorMax(const std::vector<float> &values, size_t start, size_t end)
{
    float maxValue = -std::numeric_limits<float>::max();
    for (size_t i = start; i < end; ++i)
    {
        float v = values[i];
        if (v > maxValue)
        {
            maxValue = v;
        }
    }
    return maxValue;
}

static inline bool float_equal(float v1, float v2, float tolerance)
{
    return std::abs(v2 - v1) <= tolerance;
}

template <typename T>
bool CheckFrequencyResponse(T &filter, float f, float expectedDb, float magnitude = 1.0, float sampleRate = DEFAULT_SAMPLE_RATE, float toleranceDb = 1)
{
    filter.Initialize(sampleRate);
    std::vector<float> input = MakeSinWave(sampleRate, 1, f, magnitude);
    std::vector<float> output(input.size());
    if (f == 0)
    {
        for (size_t i = 0; i < input.size(); ++i)
        {
            input[i] = magnitude;
        }
    }
    for (size_t i = 0; i < input.size(); ++i)
    {
        output[i] = filter.Tick(input[i]);
    }
    float min = VectorMin(output, output.size() / 10, output.size());
    float max = VectorMax(output, output.size() / 10, output.size());
    constexpr float TOLERANCE = 0.001;
    return (float_equal(Af2Db(-min), expectedDb, toleranceDb)) && (float_equal(Af2Db(max), expectedDb, toleranceDb));
}

static void ApplyHannWindow(std::vector<float> &values)
{
    size_t N = values.size();
    for (size_t i = 0; i < N; ++i)
    {
        float w = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi * i / (N - 1)));
        values[i] *= w;
    }
}

static double SignalPower(const std::vector<float> &values)
{
    double sum = 0;
    for (float v : values)
    {
        sum += v * v;
    }
    return std::sqrt(sum / values.size());
}

double GetFrequencyResponse(const std::function<float(float)> &filter, float f, float magnitude = 0.1, float sampleRate = DEFAULT_SAMPLE_RATE)
{
    std::vector<float> input = MakeSinWave(sampleRate, 1, f, magnitude);
    ApplyHannWindow(input);
    std::vector<float> output(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        output[i] = filter(input[i]);
    }

    double amplitude = SignalPower(output) / SignalPower(input);
    return Af2Db((float)amplitude);
}

TEST_CASE("ToobCompressor", "[ToobCompressor]")
{
    SECTION("InputStage")
    {
        OtaInputStage inputStage;
        inputStage.Initialize(DEFAULT_SAMPLE_RATE);

        float expectedPeak = (2.4 - 0.62) * 0.5 * 0.5 * inputStage.Trim();
        REQUIRE(CheckFrequencyResponse(inputStage, 1000, Af2Db(expectedPeak), 0.5));
    }
    SECTION("EmphasisStage")
    {

        // measured AC response from Spice Sims.
        std::vector<std::pair<float, float>> simulationResponse = {
            // Hz, dB
            {10, -54.45},
            {44.23, -41.31},
            {98.91, -39.76},
            {213.56, -39.27},
            {498.88, -38.47},
            {995.61, -36.54},
            {995.61, -36.54},
            {1995.6, -32.81},
            {3090.4, -29.89},
            {6500.0, -25.26},
            {6500.0, -25.26},
            {9847, -23.43},
            {17850, -21.96},
        };
        ClaudeEmphasisFilter emphasisStage;
        emphasisStage.SetSampleRate(DEFAULT_SAMPLE_RATE);

        bool result = true;

        using namespace std;

        ofstream file("/tmp/emphasis.csv");
        file << "Frequency,Expected,Actual\n";
        for (auto &[f, expectedDb] : simulationResponse)
        {
            float actualDb = GetFrequencyResponse([&](float v)
                                                  { return emphasisStage.Tick(v); }, f);
            bool good = float_equal(actualDb, expectedDb, 2.0) || f < 50.0;
            if (!good)
            {
                result = false;
            }
            std::cout << "EmphasisStage: " << f << " Hz: expected " << expectedDb << " dB, actual " << actualDb << " dB "
                      << std::abs(actualDb - expectedDb) << " dB error"
                      << (good ? "" : " FAILURE!")
                      << std::endl;
            file << f << "," << expectedDb << "," << actualDb << "\n";
        }
        REQUIRE(result);
    }
    SECTION("OtaCompressor")
    {
    }
}
