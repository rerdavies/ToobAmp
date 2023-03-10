/*
 * MIT License
 *
 * Copyright (c) 2023 Robin E. R. Davies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ConvolutionReverb.hpp"
#include "FftConvolution.hpp"
#include "BalancedFft.hpp"
#include <iostream>
#include "Fft.hpp"
#include <cmath>
#include <numbers>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "../ss.hpp"

#ifdef WITHGPERFTOOLS
#include <gperftools/profiler.h>
#endif

#pragma GCC diagnostic ignored "-Wunused-function"

using namespace LsNumerics;
using namespace std;

#define TEST_ASSERT(x)                                           \
    {                                                            \
        if (!(x))                                                \
        {                                                        \
            throw std::logic_error(SS("Assert failed: " << #x)); \
        }                                                        \
    }

static bool IsProfiling()
{
#ifdef WITHGPERFTOOLS
    return true;
#else
    return false;
#endif
}

void UsePlanCache()
{
    BalancedConvolution::SetPlanFileDirectory("fftplans");
    if (!BalancedConvolutionSection::PlanFileExists(64))
    {
        BalancedConvolution::SetPlanFileDirectory("");
        std::cout << "Plan cache files not installed." << std::endl;
        return;
    }
}
void DisablePlanCache()
{
    BalancedConvolution::SetPlanFileDirectory("");
}

static void TestBalancedFft(FftDirection direction)
{

    for (size_t n : {
             256, 8, 16, 32, 64, 128, 256, 512, 1024
#ifdef NDEBUG
             ,
             2048,
             4096, 1024 * 64
#endif
         })
    {
        cout << "=== TestBalancedFft (" << n
             << ", " << ((direction == FftDirection::Forward) ? "Forward" : "Reverse")
             << ") ======" << endl;

        BalancedFft fft(n, direction);

        cout << "MaxDelay: " << fft.Delay() << endl;

        std::vector<fft_complex_t> input;
        input.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            input[i] = i + 1;
        }
        std::vector<fft_complex_t> input2;
        input2.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            input2[i] = -i - 1;
        }

        Fft<double> normalFft{(int)n};

        std::vector<fft_complex_t> expectedOutput;
        expectedOutput.resize(n);
        std::vector<fft_complex_t> expectedOutput2;
        expectedOutput2.resize(n);
        if (direction == FftDirection::Forward)
        {
            normalFft.forward(input, expectedOutput);
            normalFft.forward(input2, expectedOutput2);
        }
        else
        {
            normalFft.backward(input, expectedOutput);
            normalFft.backward(input2, expectedOutput2);
        }

        std::vector<fft_complex_t> output;

        output.resize(n);
        std::vector<fft_complex_t> output2;
        output2.resize(n);

        ptrdiff_t outputIndex = -(ptrdiff_t)fft.Delay();

#ifndef NDEBUG
        fft.PrintPlan("/tmp/plan.txt");
#endif
        for (size_t i = 0; i < input.size(); ++i)
        {
            fft_complex_t result = fft.Tick(input[i]);

            if (outputIndex >= 0 && outputIndex < (ptrdiff_t)input.size())
            {
                assert(!std::isnan(result.real()));
                output[outputIndex] = result;
            }
            else if (outputIndex >= (ptrdiff_t)input.size() && outputIndex < (ptrdiff_t)(input.size() + input2.size()))
            {
                output2[outputIndex - input.size()] = result;
            }
            else
            {
                TEST_ASSERT(result == std::complex<double>(0));
            }
            ++outputIndex;
        }
        for (size_t i = 0; i < input2.size(); ++i)
        {
            fft_complex_t result = fft.Tick(input2[i]);

            if (outputIndex >= 0 && outputIndex < (ptrdiff_t)input.size())
            {
                assert(!std::isnan(result.real()));
                output[outputIndex] = result;
            }
            else if (outputIndex >= (ptrdiff_t)input.size() && outputIndex < (ptrdiff_t)(input.size() + input2.size()))
            {
                output2[outputIndex - input.size()] = result;
            }
            ++outputIndex;
        }
        for (size_t i = 0; i < fft.Delay(); ++i)
        {
            fft_complex_t result = fft.Tick(std::nan(""));
            if (outputIndex >= 0 && outputIndex < (ptrdiff_t)input.size())
            {
                assert(!std::isnan(result.real()));
                output[outputIndex] = result;
            }
            else if (outputIndex >= (ptrdiff_t)input.size() && outputIndex < (ptrdiff_t)(input.size() + input2.size()))
            {
                output2[outputIndex - input.size()] = result;
            }
            ++outputIndex;
        }

        for (size_t i = 0; i < n; ++i)
        {
            auto diff = expectedOutput[i] - output[i];
            auto error = std::abs(diff);
            if (error > 1E-2)
            {
                throw std::logic_error("FFT accuracy failed.");
            }
        }
        for (size_t i = 0; i < n; ++i)
        {
            auto diff = expectedOutput2[i] - output2[i];
            auto error = std::abs(diff);
            if (error > 1E-2)
            {
                throw std::logic_error("FFT accuracy failed.");
            }
        }
        // benchmark.
    }
}

class NaturalConvolutionSection
{
public:
    NaturalConvolutionSection(size_t size, std::vector<float> &audio)
        : size(size),
          fft(size * 2)
    {
        std::vector<fft_complex_t> impulse;
        impulse.resize(size * 2);
        const float norm = (float)(std::sqrt(2 * size));

        for (size_t i = 0; i < size; ++i)
        {
            float t = (i >= audio.size() ? 0 : audio[i]);

            impulse[i + size] = norm * t;
        }
        buffer.resize(impulse.size());
        outputBuffer.resize(impulse.size());

        convolutionData.resize(impulse.size());
        fft.forward(impulse, convolutionData);
    }

    void Convolve(std::vector<float> &data, std::vector<float> &output)
    {
        assert(data.size() == size * 2);
        assert(output.size() == size);

        fft.forward(data, buffer);
        for (std::size_t i = 0; i < buffer.size(); ++i)
        {
            buffer[i] *= convolutionData[i];
        }
        fft.backward(buffer, this->outputBuffer);
        for (size_t i = 0; i < size; ++i)
        {
            output[i] = this->outputBuffer[i].real();
        }
    }

private:
    std::size_t size;
    Fft<fft_float_t> fft;
    std::vector<fft_complex_t> buffer;
    std::vector<fft_complex_t> outputBuffer;
    std::vector<fft_complex_t> convolutionData;
};

static void TestBalancedConvolutionSequencing()
{
    // ensure that Sections are correctly sequenced and delayed.
    std::cout << "=== TestBalancedConvolutionSequencing ===" << std::endl;
    constexpr size_t TEST_SIZE = 65536 + 3918;

    UsePlanCache();

    std::vector<float> impulseResponse;
    impulseResponse.resize(TEST_SIZE);
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        impulseResponse[i] = i;
    }

    std::vector<float> inputValues;
    inputValues.resize(TEST_SIZE);
    inputValues[0] = 1;

    BalancedConvolution convolution(impulseResponse);
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        float result = convolution.Tick(inputValues[i]);
        float expected = impulseResponse[i];
        float error = std::abs(result - expected);
        if (expected > 1)
        {
            error /= expected;
        }

        TEST_ASSERT(error < 1E4);
    }
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        float result = convolution.Tick(inputValues[i]);
        float expected = impulseResponse[i];
        float error = std::abs(result - expected);
        if (expected > 1)
        {
            error /= expected;
        }

        TEST_ASSERT(error < 1E4);
    }

    DisablePlanCache();
}
static void TestBalancedConvolution()
{
    for (size_t n : {
             0,
             1,
             2,
             4,
             64 + 10,
             128 + 10,
             256 + 10,
             512 + 10,
             1024 + 10
#ifdef NDEBUG
             ,
             2048 + 2047,
             4095 + 512,
             16384 + 512,
#endif
         })
    {
        std::cout << "=== TestConvolution(" << n << ") ===" << std::endl;
        struct Tap
        {
            size_t delay;
            float scale;
        };
        std::vector<Tap> testTaps{

            {0, 100 * 100},
            {59, 100},
            {100, 1.0},
            {170, 0.01},
            {270, -1},
            {271, 2},
            {271, -3},
            {511, 6},
            {1029, 2.5},
            {2053, 1.2},
            {4093, -0.923},
            {9093, -1.923},
            {19093, 3.923},
            {38093, 6.923},

            {9093, -1.923},
        };

        // generate impulse that will generate data we can easily verify.
        std::vector<float> impulseData;
        impulseData.resize(n);
        for (auto &testTap : testTaps)
        {
            if (testTap.delay < impulseData.size())
            {
                impulseData[testTap.delay] = testTap.scale;
            }
        }

        std::vector<float> testData;
        testData.resize(n * 4);
        for (size_t i = 0; i < testData.size(); ++i)
        {
            testData[i] = i + 1;
        }

        auto expected{[&testTaps, &testData](size_t offset)
                      {
                          float result = 0;
                          for (auto &testTap : testTaps)
                          {
                              if (testTap.delay < offset)
                              {
                                  float data = testData[offset - testTap.delay];
                                  result += data * testTap.scale;
                              }
                          }
                          return result;
                      }};

        BalancedConvolution convolution{n, impulseData};

        for (size_t i = 0; i < testData.size(); ++i)
        {
            float expectedValue = expected(i);
            float actualValue = convolution.Tick(testData[i]);
            float error = expectedValue / actualValue;
            if (expectedValue > 1)
            {
                error /= expectedValue;
            }
            if (std::abs(error) > 1E-4)
            {
                throw logic_error("BalancedConvolutionTest failed.");
            }
        }
    }
}

class StreamCapturer
{
public:
    StreamCapturer(size_t start, size_t end = std::numeric_limits<size_t>::max())
        : start(start), end(end), index(0)
    {
    }
    StreamCapturer &operator<<(float value)
    {
        if (index >= start && index < end)
        {
            buffer.push_back(value);
        }
        ++index;
        return *this;
    }
    StreamCapturer &operator<<(const std::vector<float> &values)
    {
        for (auto i = values.begin(); i != values.end(); ++i)
        {
            (*this) << (*i);
        }
        return *this;
    }

    const std::vector<float> &Buffer() const { return buffer; }

private:
    std::vector<float> buffer;
    size_t start, end, index;
};

static void TestBalancedConvolutionSection(bool useCache)
{
    std::vector<size_t> convolutionSizes = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048
#ifndef DEBUG
                                            ,
                                            4096, 1024 * 64
#endif
    };
    if (useCache)
    {

        UsePlanCache();
        convolutionSizes = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8 * 1024, 16 * 1024, 32 * 1024, 64 * 1024};
    }
    else
    {
        DisablePlanCache();
    }

    for (size_t n : convolutionSizes)
    {
        cout << "=== TestBalancedConvolutionSection ("
             << n
             << ") "
             << ((useCache) ? "(cached)" : "(uncached)")
             << " ======" << endl;

        std::vector<float> impulseResponse;
        impulseResponse.resize(n);
        impulseResponse[0] = 10000;
        impulseResponse[1] = 100;
        if (n > 2)
        {
            impulseResponse[2] = 1;
            impulseResponse[3] = 0.01;
        }

        std::vector<float> input;
        input.resize(n * 6);
        for (size_t i = 0; i < input.size(); ++i)
        {
            input[i] = i + 1;
        }

        NaturalConvolutionSection section((size_t)n, impulseResponse);

        std::vector<float> buffer;
        buffer.resize(n * 2);
        std::vector<float> buffer2;
        buffer2.resize(n);
        StreamCapturer s0(0);

        for (size_t offset = 0; offset < input.size() - n; offset += n)
        {
            for (size_t i = 0; i < n * 2; ++i)
            {
                ptrdiff_t index = offset + i - n;
                if (index < 0)
                {
                    buffer[0] = 0;
                }
                else
                {
                    buffer[i] = input[index];
                }
            }
            section.Convolve(buffer, buffer2);
            s0 << buffer2;
        }
        const std::vector<float> &expectedOutput = s0.Buffer();

        BalancedConvolutionSection convolutionSection(n, impulseResponse);
        cout << "MaxDelay: " << convolutionSection.Delay() << endl;
#ifndef NDEBUG
        convolutionSection.PrintPlan("/tmp/plan.txt");
#endif

        // convolutionSection.PrintPlan();

        std::vector<float> t;

        StreamCapturer streamResult(convolutionSection.Delay());

        for (size_t i = 0; i < expectedOutput.size() + convolutionSection.Delay(); ++i)
        {
            float result = convolutionSection.Tick(i < input.size() ? input[i] : 0);
            streamResult << result;
            t.push_back(result);
        }

        const std::vector<float> &output = streamResult.Buffer();

        for (size_t i = 0; i < output.size(); ++i)
        {
            auto error = std::abs(expectedOutput[i] - output[i]);
            if (std::abs(expectedOutput[i]) > 1)
            {
                error /= expectedOutput[i];
            }
            if (error > 1E-4)
            {
                throw std::logic_error("BalancedConvolutionTest failed.");
            }
        }
    }
    DisablePlanCache();
}
static void TestDirectConvolutionSection()
{
    std::vector<size_t> convolutionSizes = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048
#ifndef DEBUG
                                            ,
                                            4096, 1024 * 64
#endif
    };
    for (size_t n : convolutionSizes)
    {
        cout << "=== TestDirectConvolutionSection ("
             << n
             << ") "
             << " ======" << endl;

        std::vector<float> impulseResponse;
        impulseResponse.resize(n);
        impulseResponse[0] = 10000;
        impulseResponse[1] = 100;
        if (n > 2)
        {
            impulseResponse[2] = 1;
            impulseResponse[3] = 0.01;
        }

        std::vector<float> input;
        input.resize(n * 6);
        for (size_t i = 0; i < input.size(); ++i)
        {
            input[i] = i + 1;
        }

        NaturalConvolutionSection naturalSection((size_t)n, impulseResponse);

        std::vector<float> buffer;
        buffer.resize(n * 2);
        std::vector<float> buffer2;
        buffer2.resize(n);
        StreamCapturer s0(0);

        for (size_t offset = 0; offset < input.size() - n; offset += n)
        {
            for (size_t i = 0; i < n * 2; ++i)
            {
                ptrdiff_t index = offset + i - n;
                if (index < 0)
                {
                    buffer[0] = 0;
                }
                else
                {
                    buffer[i] = input[index];
                }
            }
            naturalSection.Convolve(buffer, buffer2);
            s0 << buffer2;
        }
        const std::vector<float> &expectedOutput = s0.Buffer();

        Implementation::DirectConvolutionSection convolutionSection(n, 0, impulseResponse);
        cout << "MaxDelay: " << convolutionSection.Delay() << endl;

        // convolutionSection.PrintPlan();

        std::vector<float> t;

        StreamCapturer streamResult(convolutionSection.Delay());

        for (size_t i = 0; i < expectedOutput.size() + convolutionSection.Delay(); ++i)
        {
            float result = convolutionSection.Tick(i < input.size() ? input[i] : 0);
            streamResult << result;
            t.push_back(result);
        }

        const std::vector<float> &output = streamResult.Buffer();

        for (size_t i = 0; i < output.size(); ++i)
        {
            auto error = std::abs(expectedOutput[i] - output[i]);
            if (std::abs(expectedOutput[i]) > 1)
            {
                error /= expectedOutput[i];
            }
            if (error > 1E-4)
            {
                throw std::logic_error("DirectConvolutionTest failed.");
            }
        }
    }
    DisablePlanCache();
}

static size_t NextPowerOf2(size_t size)
{
    size_t result = 1;
    while (result < size)
    {
        result *= 2;
    }
    return result;
}
void BenchmarkBalancedConvolution()
{
    UsePlanCache();

    // std::vector<double> impulseTimes = {0.01, 0.1, 1.0};
    std::vector<double> impulseTimes = {1.0};

    for (auto impulseTimeSeconds : impulseTimes)
    {
        std::cout << "=== Balanced Convolution benchmark " << impulseTimeSeconds << "sec =====" << endl;
        size_t sampleRate = 44100;

        double benchmarkTimeSeconds = 22.0;
        size_t impulseSize = (size_t)sampleRate * impulseTimeSeconds;

        std::vector<float> impulseData;
        impulseData.resize(impulseSize);
        for (size_t i = 0; i < impulseSize; ++i)
        {
            impulseData[i] = i / (float)impulseSize;
        }

        size_t bufferSize = 64;
        std::vector<float> inputBuffer;
        std::vector<float> outputBuffer;
        inputBuffer.resize(bufferSize);
        outputBuffer.resize(bufferSize);
        for (size_t i = 0; i < bufferSize; ++i)
        {
            inputBuffer[i] = i / (float)bufferSize;
        }

        BalancedConvolution convolver(impulseData);

        size_t nSamples = (size_t)(sampleRate * benchmarkTimeSeconds);

        using clock = std::chrono::steady_clock;

#ifdef WITHGPERFTOOLS
        ProfilerStart("/tmp/out.prof");
#endif
        auto startTime = clock::now();
        for (size_t i = 0; i < nSamples; i += bufferSize)
        {
            convolver.Tick(inputBuffer, outputBuffer);
        }
        auto endTime = clock::now();
        auto diff = endTime - startTime;
        using second_duration = std::chrono::duration<float>;
        second_duration seconds = endTime - startTime;
#ifdef WITHGPERFTOOLS
        ProfilerStop();
#endif

        double percent = seconds.count() / benchmarkTimeSeconds * 100;

        std::cout << "Performance (percent of realtime): " << percent << "%" << std::endl;

        if (!IsProfiling())
        {
            size_t size = NextPowerOf2(impulseData.size());
            NaturalConvolutionSection section(size, impulseData);
            std::vector<float> t;
            t.resize(size * 2);
            outputBuffer.resize(size);

            size_t nativeSamples = 0;
            startTime = clock::now();
            for (size_t i = 0; i < nSamples; i += size)
            {
                section.Convolve(t, outputBuffer);
                nativeSamples += size;
            }
            endTime = clock::now();
            diff = endTime - startTime;
            seconds = endTime - startTime;

            percent = seconds.count() / benchmarkTimeSeconds * nativeSamples / nSamples * 100;
            cout << "Natural fft time: " << percent << "%" << endl;
        }
    }
    DisablePlanCache();
}

void TestFftConvolution()
{
    for (size_t n : {

             (size_t)0,
             (size_t)1,
             FftConvolution::MINIMUM_DIRECT_CONVOLUTION_LENGTH,
             FftConvolution::MINIMUM_DIRECT_CONVOLUTION_LENGTH + 64 + 10,
             (size_t)5535, // section sizes: .... 2048,1024
             (size_t)7034, // section sizes: .... 2048,2048
#ifdef NDEBUG
             (size_t)55134,
#endif
         })
    {
        cout << "TestFftConvolution " << n << " ===" << endl;
        std::vector<float> impulse;
        impulse.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            impulse[i] = i + 1;
        }
        FftConvolution convolver(impulse);

        std::vector<float> samples;
        samples.resize(n);
        if (n == 0)
        {
            for (size_t i = 0; i < 100; ++i)
            {
                float result = convolver.Tick(99);
                if (result != 0)
                {
                    throw std::logic_error("TestFftConvolution failed.");
                }
            }
        }
        else
        {
            samples[0] = 1;

            size_t sampleIndex = 0;
            for (size_t i = 0; i < n * 4; ++i)
            {
                float result = convolver.Tick(samples[sampleIndex]);
                float error = (result - impulse[sampleIndex]);

                if (std::abs(error) >= 1E-4)
                {
                    std::logic_error("TestFftConvolution failed.");
                }
                ++sampleIndex;
                if (sampleIndex == samples.size())
                {
                    sampleIndex = 0;
                }
            }
        }
    }
}

void TestFftConvolutionBenchmark(bool profiling = false)
{
    std::cout << "=== Fft Convolution benchmark =====" << endl;
    size_t sampleRate = 48000;

    double benchmarkTimeSeconds = 4.0;
    double impulseTimeSeconds = 1.0;
    size_t impulseSize = (size_t)sampleRate * impulseTimeSeconds;

    std::vector<float> impulseData;
    impulseData.resize(impulseSize);
    for (size_t i = 0; i < impulseSize; ++i)
    {
        impulseData[i] = i / (float)impulseSize;
    }

    size_t bufferSize = 64;
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    inputBuffer.resize(bufferSize);
    outputBuffer.resize(bufferSize);
    for (size_t i = 0; i < bufferSize; ++i)
    {
        inputBuffer[i] = i / (float)bufferSize;
    }

    FftConvolution convolver(impulseData);

    size_t nSamples = (size_t)(sampleRate * benchmarkTimeSeconds);

    using clock = std::chrono::steady_clock;

    auto startTime = clock::now();
    for (size_t i = 0; i < nSamples; i += bufferSize)
    {
        convolver.Tick(inputBuffer, outputBuffer);
    }
    auto endTime = clock::now();
    auto diff = endTime - startTime;
    using second_duration = std::chrono::duration<float>;
    second_duration seconds = endTime - startTime;

    double percent = seconds.count() / benchmarkTimeSeconds * 100;

    std::cout << "Performance (percent of realtime): " << percent << "%" << std::endl;

    if (!profiling)
    {

        size_t size = NextPowerOf2(impulseData.size());
        NaturalConvolutionSection section(size, impulseData);
        std::vector<float> t;
        t.resize(size * 2);
        outputBuffer.resize(size);

        size_t naturalSamples = 0;
        startTime = clock::now();
        for (size_t i = 0; i < nSamples; i += size)
        {
            section.Convolve(t, outputBuffer);
            naturalSamples += size;
        }
        endTime = clock::now();
        diff = endTime - startTime;
        seconds = endTime - startTime;

        percent = seconds.count() / (benchmarkTimeSeconds * naturalSamples / nSamples) * 100;
        cout << "Natural fft time: " << percent << "%" << endl
             << endl;
    }
}

static void Consume(double value)
{
}

void BenchmarkFftConvolutionStep()
{

    // std::stringstream ss;

    constexpr size_t FRAMES = 8 * 1024 * 1024;

    size_t frames = FRAMES;

    std::ostream &ss = std::cout;

    ss << std::left << std::setw(8) << "N"
       << " " << std::setw(12) << "fft"
       << " " << std::setw(12) << "balanced"
       << " " << std::setw(12) << "naive"
       << " " << std::setw(12) << "seconds"
       << " " << std::setw(12) << "cycles"
       << " " << std::setw(12) << "delay"
       << setw(0) << std::left << endl;
    ss << "-------------------------------------------------------------------------------------" << endl;

    for (size_t n : {
             4,
             8,
             16,
             32,
             64,
             128,
             256,
             512,
             1024,
             2048,
             4096,
             8 * 1024,
             16 * 1024,
             32 * 1024,
             64 * 1024,
             128 * 1024})
    {

        using clock_t = std::chrono::high_resolution_clock;
        using ns_duration_t = std::chrono::nanoseconds;

        std::vector<float> impulse;
        impulse.resize(n * 2);
        for (size_t i = 0; i < n * 2; ++i)
        {
            impulse[i] = i / double(n * 2);
        }
        FftConvolution::Section fftSection(n, 0, impulse);

        std::vector<float> input;
        input.resize(n);

        // FftConvolution
        FftConvolution::DelayLine delayLine(n * 2);

        auto start = clock_t::now();
        for (size_t frame = 0; frame < frames; ++frame)
        {
            for (size_t i = 0; i < n; ++i)
            {
                delayLine.push(input[i]);
                fftSection.Tick(delayLine);
            }
        }
        ns_duration_t fftConvolutionDuration = std::chrono::duration_cast<ns_duration_t>(clock_t::now() - start);

        // balanced convolution.
        BalancedConvolutionSection balancedSection(n, impulse);

        auto bStart = clock_t::now();
        for (size_t frame = 0; frame < frames; ++frame)
        {
            for (size_t i = 0; i < n; ++i)
            {
                balancedSection.Tick(input[i]);
            }
        }
        ns_duration_t bConvolutionDuration = std::chrono::duration_cast<ns_duration_t>(clock_t::now() - bStart);

        // Naive convolution.
        bool showNaive = n <= 1024;
        ns_duration_t nConvolutionMs;
        if (showNaive)
        {
            auto nStart = clock_t::now();
            double sink = 0;
            for (size_t frame = 0; frame < frames; ++frame)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    for (size_t i = 0; i < n; ++i)
                    {
                        delayLine.push(input[i]);
                        double result = 0;
                        for (size_t k = 0; k < n; ++k)
                        {
                            result += delayLine[k] * impulse[k];
                        }
                        sink = result;
                    }

                    fftSection.Tick(delayLine);
                }
            }
            nConvolutionMs = std::chrono::duration_cast<ns_duration_t>(clock_t::now() - nStart);
            Consume(sink);
        }

        size_t samples = frames * n;
        double scale = 1.0 / samples;

        double seconds = std::chrono::duration_cast<std::chrono::duration<float>>(bConvolutionDuration).count();

        ss << std::setprecision(3)
           << std::setiosflags(std::ios_base::fixed)
           << std::left << std::setw(8) << n
           << " " << std::setw(12) << std::right << fftConvolutionDuration.count() * scale
           << " " << std::setw(12) << std::right << bConvolutionDuration.count() * scale
           << " " << std::setw(12) << std::right;

        if (showNaive)
        {
            ss << nConvolutionMs.count() * scale;
        }
        else
        {
            ss << " ";
        }

        ss
            << std::setw(12) << std::right << seconds
            << std::setw(12) << std::right << samples;

        ss
            << std::setw(12) << std::right << balancedSection.Delay();
        ss
            << setw(0) << endl;
        // reduce iterations if our measurement took too long.
        while (seconds > 4)
        {
            frames /= 2;
            seconds /= 2;
        }
    }

    // std::cout << ss.str();
    // std::cout.flush();
}

void TestFft()
{
    BenchmarkBalancedConvolution();

    TestDirectConvolutionSection();
    TestBalancedConvolutionSequencing();
    TestBalancedConvolution();

    Implementation::SlotUsageTest();

    TestBalancedConvolutionSection(true);
    TestBalancedConvolutionSection(false);

    BenchmarkFftConvolutionStep();

    // TestBalancedFft(FftDirection::Reverse);
    // TestBalancedFft(FftDirection::Forward);

    // TestBalancedConvolution();
    // BenchmarkBalancedConvolution();

    // TestFftConvolution();
    // TestFftConvolutionBenchmark();
}

int main(int argc, char **argv)
{
    try
    {
        if (IsProfiling())
        {
            //BenchmarkFftConvolutionStep();
            BenchmarkBalancedConvolution();
            return EXIT_SUCCESS;
        }
        else
        {
            TestFft();
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "TEST FAILED: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}