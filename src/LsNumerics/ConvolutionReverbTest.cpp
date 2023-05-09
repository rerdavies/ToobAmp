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

#include "FftConvolution.hpp"
#include "BalancedConvolution.hpp"
#include <iostream>
#include "StagedFft.hpp"
#include <cmath>
#include <numbers>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "../ss.hpp"
#include "../CommandLineParser.hpp"
#include "LagrangeInterpolator.hpp"
#include "../AudioData.hpp"
#include "../WavReader.hpp"
#include "../WavWriter.hpp"

#include <time.h> // for clock_nanosleep

#ifdef WITHGPERFTOOLS
#include <gperftools/profiler.h>
#endif

#pragma GCC diagnostic ignored "-Wunused-function"

using namespace LsNumerics;
using namespace std;
using namespace toob;

std::ostream &output = cout;

#define TEST_ASSERT(x)                                           \
    {                                                            \
        if (!(x))                                                \
        {                                                        \
            throw std::logic_error(SS("Assert failed: " << #x)); \
        }                                                        \
    }

bool shortTests = false;
bool buildTests = false;
static std::string profilerFileName;

static bool IsProfiling()
{
#ifdef WITHGPERFTOOLS
    return profilerFileName.length() != 0;
#else
    return false;
#endif
}

static float RelError(float expected, float actual)
{
    float error = std::abs(expected - actual);
    float absExpected = std::abs(expected);
    if (absExpected > 1)
    {
        error /= absExpected;
    }
    return error;
}

class ClockSleeper
{
public:
    static constexpr clockid_t CLOCK = CLOCK_MONOTONIC;

    ClockSleeper()
    {
        struct timespec resolution;
        clock_getres(CLOCK, &resolution);
        clock_gettime(CLOCK, &currentTime);
    }

    void Sleep(int64_t nanoseconds)
    {
        currentTime.tv_nsec += nanoseconds;
        while (nanoseconds + currentTime.tv_nsec >= 1000000000)
        {
            ++currentTime.tv_sec;
            nanoseconds -= 1000000000;
        }
        currentTime.tv_nsec += (long)nanoseconds;

        struct timespec remaining;

        clock_nanosleep(CLOCK, TIMER_ABSTIME, &currentTime, &remaining);
    }

private:
    timespec currentTime;
};

void UsePlanCache()
{
    if (buildTests)
        return;
    BalancedConvolution::SetPlanFileDirectory("fftplans");
    if (!BalancedConvolutionSection::PlanFileExists(64))
    {
        BalancedConvolution::SetPlanFileDirectory("");
        std::cout << "Plan cache files not installed." << std::endl;
        std::cout << "Run \'build/src/GenerateFftPlans fftplans in the project root." << std::endl;
        std::cout << "Warning: requires at 8GB of memory to run!" << std::endl;
        throw std::logic_error("Can't continue.");
    }
}

void DisablePlanCache()
{
    if (buildTests)
        return;
    BalancedConvolution::SetPlanFileDirectory("");
}

class PlanCacheGuard
{
public:
    PlanCacheGuard() { UsePlanCache(); }
    ~PlanCacheGuard() { DisablePlanCache(); }
};

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

        StagedFft normalFft = StagedFft(n);

        std::vector<fft_complex_t> expectedOutput;
        expectedOutput.resize(n);
        std::vector<fft_complex_t> expectedOutput2;
        expectedOutput2.resize(n);
        if (direction == FftDirection::Forward)
        {
            normalFft.Compute(input, expectedOutput, StagedFft::Direction::Forward);
            normalFft.Compute(input2, expectedOutput2, StagedFft::Direction::Forward);
        }
        else
        {
            normalFft.Compute(input, expectedOutput, StagedFft::Direction::Backward);
            normalFft.Compute(input2, expectedOutput2, StagedFft::Direction::Backward);
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
        fft.Compute(impulse, convolutionData, StagedFft::Direction::Forward);
    }

    void Convolve(std::vector<float> &data, std::vector<float> &output)
    {
        assert(data.size() == size * 2);
        assert(output.size() == size);

        fft.Compute(data, buffer, StagedFft::Direction::Forward);
        for (std::size_t i = 0; i < buffer.size(); ++i)
        {
            buffer[i] *= convolutionData[i];
        }
        fft.Compute(buffer, this->outputBuffer, StagedFft::Direction::Backward);
        for (size_t i = 0; i < size; ++i)
        {
            output[i] = this->outputBuffer[i].real();
        }
    }

private:
    std::size_t size;
    StagedFft fft;
    std::vector<fft_complex_t> buffer;
    std::vector<fft_complex_t> outputBuffer;
    std::vector<fft_complex_t> convolutionData;
};

static void TestBalancedConvolutionSequencing()
{

    PlanCacheGuard guard;

    // ensure that Sections are correctly sequenced and delayed.
    std::cout << "=== TestBalancedConvolutionSequencing ===" << std::endl;
    size_t TEST_SIZE = 65536 + 3918;
    if (buildTests)
    {
        TEST_SIZE = 3048;
    }

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

    BalancedConvolution convolution(SchedulerPolicy::UnitTest, impulseResponse);
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        float result = convolution.Tick(inputValues[i]);
        float expected = impulseResponse[i];
        float error = std::abs(result - expected);
        if (expected > 1)
        {
            error /= expected;
        }

        TEST_ASSERT(error < 1E-4);
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

        TEST_ASSERT(error < 1E-4);
    }

    DisablePlanCache();
}
static void TestBalancedConvolution()
{
    UsePlanCache();
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
        std::cout << "=== TestBalancedConvolution(" << n << ") ===" << std::endl;
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

        BalancedConvolution convolution{SchedulerPolicy::UnitTest, n, impulseData};

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
    DisablePlanCache();
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
    std::vector<size_t> convolutionSizes = {64, 128, 256, 512, 1024, 2048
#ifndef DEBUG
                                            ,
                                            4096
#endif
    };
    if (buildTests)
    {
        convolutionSizes = {64, 128, 256};
    }
    if (useCache)
    {

        UsePlanCache();
        convolutionSizes = {64, 128, 256, 512, 1024, 2048, 4096,};
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
    std::vector<size_t> convolutionSizes = {8, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048
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
        cout << "MaxDelay: " << convolutionSection.SectionDelay() << endl;

        // convolutionSection.PrintPlan();

        std::vector<float> t;

        StreamCapturer streamResult(convolutionSection.SectionDelay());

        for (size_t i = 0; i < expectedOutput.size() + convolutionSection.SectionDelay(); ++i)
        {
            float result = convolutionSection.Tick(i < input.size() ? input[i] : 0);
            streamResult << result;
            t.push_back(result);
        }

        const std::vector<float> &output = streamResult.Buffer();

        size_t delay = convolutionSection.Size();  // .Delay seems to be broken.

        for (size_t i = 0; i < expectedOutput.size()-delay; ++i)
        {
            auto error = std::abs(expectedOutput[i] - output[i+delay]);
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

    std::vector<double> impulseTimes = {0.1, 1.0, 2.0,4.0};
    if (buildTests)
    {
        impulseTimes = {1.0};
    }
    if (IsProfiling())
    {
        impulseTimes = {1.0};
    }

    for (auto impulseTimeSeconds : impulseTimes)
    {
        std::cout << "=== Balanced Convolution benchmark " << impulseTimeSeconds << "sec =====" << endl;
        size_t sampleRate = 44100;

        double benchmarkTimeSeconds = 22.0;
        size_t impulseSize = (size_t)sampleRate * impulseTimeSeconds;

#ifdef WITHGPERFTOOLS
        if (IsProfiling())
        {
            benchmarkTimeSeconds *= 4;
        }
#endif
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

        BalancedConvolution convolver(SchedulerPolicy::UnitTest, impulseData, 48000, bufferSize);

        size_t nSamples = (size_t)(sampleRate * benchmarkTimeSeconds);

        using clock = std::chrono::steady_clock;

#ifdef WITHGPERFTOOLS
        if (IsProfiling())
        {
            ProfilerStart(profilerFileName.c_str());
        }
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
        if (IsProfiling())
        {
            ProfilerStop();
        }
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
    UsePlanCache();
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
    DisablePlanCache();
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

double gSinkValue = 0;
static void Consume(double value)
{
    gSinkValue = value;
}

void BenchmarkFftConvolutionStep()
{
    if (buildTests)
        return;
    UsePlanCache();

    // std::stringstream ss;
#define SHORT 0

#if !SHORT
    constexpr size_t FRAMES = 8 * 1024 * 1024;
#else
    constexpr size_t FRAMES = 8 * 1024 * 1024 / 256 / 2;
#endif

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
#if !SHORT
             4,
             8,
             16,
             32,
             64,
             128,
             256,
#endif
             512,
             1024,
             2048,
             4096,
             8 * 1024,
             16 * 1024,
             32 * 1024,
             64 * 1024,
             128 * 1024,
             256 * 1024,
             512 * 1024,
             1024 * 1024              
              })
    {

        using clock_t = std::chrono::high_resolution_clock;
        using ns_duration_t = std::chrono::nanoseconds;

        std::vector<float> impulse;
        impulse.resize(n );
        for (size_t i = 0; i < n ; ++i)
        {
            impulse[i] = i / double(n * 2);
        }
        std::vector<float> input;
        input.resize(n);

        // FftConvolution
        FftConvolution::DelayLine delayLine(n * 2);

        Implementation::DirectConvolutionSection directSection(n, 0, impulse);

        ns_duration_t fftConvolutionDuration;
        auto start = clock_t::now();
        for (size_t frame = 0; frame < frames; ++frame)
        {
            for (size_t i = 0; i < n; ++i)
            {
                directSection.Tick(input[i]);
            }
        }
        fftConvolutionDuration = std::chrono::duration_cast<ns_duration_t>(clock_t::now() - start);

        // balanced convolution.
        bool doBalancedConvolution = n <= 8*1024;
        ns_duration_t bConvolutionDuration {0};
        size_t balancedSectionDelay = 0;
        if (doBalancedConvolution)
        {
            BalancedConvolutionSection balancedSection(n, impulse);
            balancedSectionDelay = balancedSection.Delay();
            auto bStart = clock_t::now();
            double sink = 0;

            for (size_t frame = 0; frame < frames; ++frame)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    sink = balancedSection.Tick(input[i]);
                }
            }
            bConvolutionDuration = std::chrono::duration_cast<ns_duration_t>(clock_t::now() - bStart);
            Consume(sink);
        }
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
                    delayLine.push(input[i]);
                    sink += delayLine.Convolve(impulse);
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
           << " " << std::setw(12) << std::right << fftConvolutionDuration.count() * scale;
        if (doBalancedConvolution)
        {
            ss 
                << " " << std::setw(12) << std::right << bConvolutionDuration.count() * scale
                << " " << std::setw(12) << std::right;
        } else {
            ss 
                << " " << std::setw(12) << std::right << " "
                << " " << std::setw(12) << std::right;

        }

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

        if (doBalancedConvolution)
        {
            ss << std::setw(12) << std::right << balancedSectionDelay;
        } else {
            ss << std::setw(12) << std::right << " ";

        }

        if (directSection.IsShuffleOptimized())
        {
            ss << " " << std::setw(12) << std::left << "Shuffle-optimized";
        } else if (directSection.IsL2Optimized())
        {
            ss << " " << std::setw(12) << std::left << "L2-optimized";
        }
        else if (directSection.IsL1Optimized())
        {
            ss << " " << std::setw(12) << std::left << "L1-optimized";
        }
        ss
            << setw(0) << endl;
        // reduce iterations if our measurement took too long.
        if (!doBalancedConvolution)
        {
            frames /= 2;
        } else {
            while (seconds > 4)
            {
                frames /= 2;
                seconds /= 2;
            }
        }
    }

    // std::cout << ss.str();
    // std::cout.flush();

    DisablePlanCache();
}

void TestDirectConvolutionSectionAllocations()
{

    if (buildTests)
        return;
    PlanCacheGuard usePlanCache;

    // test code that allows viewing of section allocations in a balanced convolution when the corresponding traces are uncommented.
    std::vector<float> impulseData;
    impulseData.resize(105);
    impulseData[1] = 1;

    for (size_t n = 15382; n < 255 * 1024; n = n * 5 / 4)
    {
        cout << "==== TestDirectConvolutionSectionAllocations(" << n << ") === " << endl;
        BalancedConvolution convolution(SchedulerPolicy::UnitTest, n, impulseData);
    }
}

static void RealtimeConvolutionCpuUse()
{
    UsePlanCache();
    for (size_t N : {
             48000 // buncha sections.
         })
    {
        cout << "==== BenchmarkRealtimeConvolution n=" << N << endl;
        cout << "Check CPU use. Press Ctrl+C to stop." << endl;

        size_t BUFFER_SAMPLES = 256;
        size_t SAMPLE_RATE = 48000;

        std::vector<float> impulse;
        impulse.resize(N);
        for (size_t i = 0; i < N; ++i)
        {
            impulse[i] = i + 1;
        }

        std::vector<float> inputData;
        inputData.resize(N);
        inputData[0] = 1;

        BalancedConvolution convolution(SchedulerPolicy::UnitTest, impulse, SAMPLE_RATE, BUFFER_SAMPLES);

        size_t inputIndex = 0;
        std::vector<float> inputBuffer;
        inputBuffer.resize(BUFFER_SAMPLES);
        std::vector<float> outputBuffer;
        outputBuffer.resize(BUFFER_SAMPLES);

        size_t sleepNanoseconds = 1000000000 * BUFFER_SAMPLES / SAMPLE_RATE;

        ClockSleeper clockSleeper;

        size_t nSample = 0;
        size_t outputIndex = 0;
        while (true)
        {
            for (size_t i = 0; i < BUFFER_SAMPLES; ++i)
            {
                inputBuffer[i] = inputData[inputIndex++];
                if (inputIndex >= inputData.size())
                {
                    inputIndex = 0;
                }
                ++nSample;
            }
            convolution.Tick(inputBuffer, outputBuffer);

            for (size_t i = 0; i < BUFFER_SAMPLES; ++i)
            {
                float expected = impulse[outputIndex++];
                if (outputIndex >= impulse.size())
                {
                    outputIndex = 0;
                }
                float actual = outputBuffer[i];

                TEST_ASSERT(RelError(expected, actual) < 1E-4);
            }
            clockSleeper.Sleep(sleepNanoseconds);
        }

        (void)nSample;
    }
    DisablePlanCache();
}

static void TestRealtimeConvolution()
{
    UsePlanCache();
    for (size_t N : {
             683 + 255, // one direct section
             939 + 511, // two direct sections,
             32554,     // buncha sections.
         })
    {
        cout << "==== TestRealtimeConvolution n=" << N << endl;

        size_t BUFFER_SAMPLES = 256;
        size_t SAMPLE_RATE = 48000;

        std::vector<float> impulse;
        impulse.resize(N);
        for (size_t i = 0; i < N; ++i)
        {
            impulse[i] = i + 1;
        }

        std::vector<float> inputData;
        inputData.resize(N);
        inputData[0] = 1;

        BalancedConvolution convolution(SchedulerPolicy::UnitTest, impulse, SAMPLE_RATE, BUFFER_SAMPLES);

        size_t inputIndex = 0;
        std::vector<float> inputBuffer;
        inputBuffer.resize(BUFFER_SAMPLES);
        std::vector<float> outputBuffer;
        outputBuffer.resize(BUFFER_SAMPLES);

        size_t sleepNanoseconds = 1000000000 * BUFFER_SAMPLES / SAMPLE_RATE;

        double seconds = 5.0;
        size_t nFrames = (size_t)((seconds * SAMPLE_RATE) / BUFFER_SAMPLES);

        ClockSleeper clockSleeper;

        size_t nSample = 0;
        size_t outputIndex = 0;
        for (size_t frame = 0; frame < nFrames; ++frame)
        {
            for (size_t i = 0; i < BUFFER_SAMPLES; ++i)
            {
                inputBuffer[i] = inputData[inputIndex++];
                if (inputIndex >= inputData.size())
                {
                    inputIndex = 0;
                }
                ++nSample;
            }
            convolution.Tick(inputBuffer, outputBuffer);

            for (size_t i = 0; i < BUFFER_SAMPLES; ++i)
            {
                float expected = impulse[outputIndex++];
                if (outputIndex >= impulse.size())
                {
                    outputIndex = 0;
                }
                float actual = outputBuffer[i];

                TEST_ASSERT(RelError(expected, actual) < 1E-4);
            }
            clockSleeper.Sleep(sleepNanoseconds);
        }
        cout << "Underruns: " << convolution.GetUnderrunCount() << endl;

        (void)nSample;
    }
    DisablePlanCache();
}

static void TestLagrangeInterpolator()
{
    cout << "=== TestLagrangeInterpolator =================" << endl;

    {
        LagrangeInterpolator interpolator{12};
        std::vector<float> inputData(36);
        inputData[10] = 1;
        auto result = interpolator.Interpolate(&(inputData.at(0)), 10);
        TEST_ASSERT(std::abs(result-1) < 1E-10);
        TEST_ASSERT(interpolator.Interpolate(&(inputData.at(0)), 9) == 0)
        TEST_ASSERT(interpolator.Interpolate(&(inputData.at(0)), 11) == 0)
    }

    for (size_t inputSampleRate : {44100, 48000, 88200, 96000})
    {
        for (size_t outputSampleRate : {44100, 48000, 88200, 96000})
        {
            if (inputSampleRate != outputSampleRate)
            {

                double worstError = 0;
                double worstPreambleError = 0;
                double maxValue = 0;
                for (double f0 : {100, 300, 600, 1000, 2000, 4000, 6000, 8000, 12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000})
                {
                    double f = f0;

                    std::vector<float> inputBuffer(32768);
                    double m = f * std::numbers::pi * 2 / inputSampleRate;
                    for (size_t i = 0; i < inputBuffer.size(); ++i)
                    {
                        inputBuffer[i] = std::cos(i * m);
                    }
                    AudioData input(inputSampleRate, inputBuffer);
                    AudioData output;
                    input.Resample(outputSampleRate, output);

                    // cursory analysis for reasonableness.
                    double maxError = 0;
                    double mOut = f * std::numbers::pi * 2 / outputSampleRate;
                    for (size_t i = 50; i < output.getSize() - 50; ++i)
                    {
                        double expected = std::cos(i * mOut);
                        double actual = output.getChannel(0)[i];
                        double err = std::abs(expected - actual);
                        if (err > maxError)
                        {
                            maxError = err;
                        }
                        if (std::abs(actual) > maxValue)
                        {
                            maxValue = std::abs(actual);
                        }
                    }

                    std::vector<float> expected(output.getSize());
                    for (size_t i = 0; i < expected.size(); ++i)
                    {
                        expected[i] = std::cos(i * mOut);
                    }
                    double preambleError = 0;
                    for (size_t i = 0; i < 50; ++i)
                    {
                        double actual = output.getChannel(0)[i];
                        double err = std::abs(expected[i] - actual);
                        if (err > preambleError)
                        {
                            preambleError = err;
                        }
                        if (std::abs(actual) > maxValue)
                        {
                            maxValue = std::abs(actual);
                        }
                    }
                    if (maxError > worstError)
                    {
                        worstError = maxError;
                    }
                    if (preambleError > worstPreambleError)
                    {
                        worstPreambleError = preambleError;
                    }

                    // // dump selected data for analysis.
                    // for (size_t i = 100; i < 200; ++i)
                    // {
                    //     cout << expected[i] << " " << output.getChannel(0)[i] << endl;
                    // }
                    //std::cout << "   in: " << inputSampleRate << " out: " << outputSampleRate << " f: " << f << " err: " << maxError << " preamble error: " << preambleError << std::endl;
                }
                //std::cout << "in: " << inputSampleRate << " out: " << outputSampleRate << " err: " << worstError << " preamble error: " << worstPreambleError << " max value: " << maxValue << std::endl;

                // Tests basic sanity.
                // Further analysis was done using fourier analysis in an excel spreadsheet. Basic results: > 20db rejection of aliasing into audible range.
                TEST_ASSERT(worstPreambleError < 3);
                TEST_ASSERT(worstError < 3);
            }
        }
    }
}
void TestFft()
{

    // If you need to isolate a particular test, add a command-line test name instead
    // of re-ordering tests here (in order to reduce potential merge conflicts).
    // see: ADD_TEST_NAME_HERE.

    TestLagrangeInterpolator();
    TestBalancedConvolution();

    Implementation::SlotUsageTest();

    TestBalancedConvolutionSequencing();

    if (!buildTests)
    {
        TestBalancedConvolutionSection(true);
    }
    TestBalancedConvolutionSection(false);

    TestFftConvolution();

    TestDirectConvolutionSectionAllocations();

    TestDirectConvolutionSection();

    TestFftConvolutionBenchmark();

    TestRealtimeConvolution();

    BenchmarkBalancedConvolution();

    BenchmarkFftConvolutionStep();

    // TestBalancedFft(FftDirection::Reverse);
    // TestBalancedFft(FftDirection::Forward);

    // TestBalancedConvolution();
    // BenchmarkBalancedConvolution();
}

static void NormalizeConvolution(AudioData & data)
{
    size_t size = data.getSize();

    for (size_t c = 0; c < data.getChannelCount(); ++c)
    {
        auto &channel = data.getChannel(c);
        double maxValue = 0;
        // find the worst-case convolution output.
        double sum = 0;
        for (size_t i = 0; i < size; ++i)
        {
            sum += channel[i];
            if (std::abs(sum) > maxValue) 
            {
                maxValue = std::abs(sum);
            }
        }
        std::cout << "MaxValue: " << maxValue << std::endl;

        float  scale = (float)(1/maxValue);

        for (size_t i = 0; i < size; ++i)
        {
            channel[i] *= scale;
        }
    }
}

void TestFile()
{
    UsePlanCache();
    WavReader reader;
    reader.Open("impulseFiles/reverb/Arthur Sykes Rymer Auditorium.wav");
    
    AudioData data;
    reader.Read(data);

    if (data.getChannelCount() == 4)
    {
        data.AmbisonicDownmix({AmbisonicMicrophone(0, 0)});
    }
    else
    {
        data.ConvertToMono();
    }

    NormalizeConvolution(data);
    cout << "Sample rate: " << data.getSampleRate() << " length: " << std::setw(4) << data.getSize()*1.0f/data.getSampleRate() << endl;
    data.Resample((size_t)48000);
    cout << "Sample rate: " << data.getSampleRate() << " length: " << std::setw(4) << data.getSize()*1.0f/data.getSampleRate() << endl;

    


    NormalizeConvolution(data);

    {
        WavWriter writer;
        writer.Open("/tmp/out.wav");
        writer.Write(data);

    }


    auto convolutionReverb = std::make_shared<ConvolutionReverb>(SchedulerPolicy::UnitTest, data.getSize(), data.getChannel(0));

    for (size_t offset = 0; offset < 20; ++offset)
    {
        std::vector<float> output;
        output.resize(data.getSize()+offset+10);
        std::vector<float> input;
        input.resize(data.getSize()+offset+10);
        input[offset] = 1.0;

        std::vector<float> inputBuffer(16);
        std::vector<float> outputBuffer(16);
        for (size_t i = 0; i < output.size(); i += 16)
        {
            size_t thisTime = input.size()-i;
            if (thisTime > 16) thisTime = 16;
            for (size_t j = 0; j < thisTime; ++j)
            {
                inputBuffer[j] = input[i+j];
            }
            convolutionReverb->Tick(16,inputBuffer,outputBuffer);
            for (size_t j = 0; j < thisTime; ++j)
            {
                output[i+j] = outputBuffer[j];
            }
        }

        for (size_t i = 0; i < offset; ++i)
        {
            if (std::abs(output[i]) > 1E-9)
            {
                cout << "offset = " << offset << " output[" << i  << "] = " << output[i] << " expected: 0" << endl;
                TEST_ASSERT(output[i] == 0);
            }
        }
        auto &expected = data.getChannel(0);
        for (size_t i = 0; i < data.getSize()-offset; ++i)
        {
            double error = std::abs(output[i+offset]-expected[i]);
            if (error> 1E3)
            {
                cout << "Error: " << error << " offset = " << offset << " output[" << i + offset << "] = " << output[i+offset] << " expected[" << i << "] = " << expected[i] << endl;
                TEST_ASSERT(output[i+offset] == data.getChannel(0)[i]);
            }
        }
    }

}


static void PrintHelp()
{
    cout << "ConvolutionReverbTest - A suite of Tests for BalancedConvultionReverb and sub-components" << endl
         << "Copyright 20202, Robin Davies" << endl
         << endl
         << "Syntax: ConvolutionReverbTest [OPTIONS] [TEST_TYPE]" << endl
         << endl
         << "Options: " << endl
         << "  --build    Run only build-machine tests." << endl
         << "  --short    Don't run long-running tests." << endl
         << "  --profile <filename>" << endl
         << "        Generate gprof profiler output to the selected filename" << endl
         << "        (for convolution_benchmark only)" << endl
         << "        e.g.  --profile /tmp/prof.out" << endl
         << "  -o ,--ouput <filename" << endl
         << "        Send test output to the specified file" << endl
         << "  -h, --help   Display this message." << endl
         << "      --plans" << endl
         << "        Display section plans." << endl
         << endl
         << "Tests: " << endl
         << "  section_benchmark:" << endl
         << "     Benchmark BalancedConvolutionSection, and DirectConvolutionSection" << endl
         << "  convolution_benchmark:" << endl
         << "     Determine percent of realtime used for basic convolutions." << endl
         << "  section_allocations: " << endl
         << "      Verify scheduling of convolution sections." << endl
         << "  check_for_stalls:" << endl
         << "       Run audio thread simulation, checking for read stalls." << endl
         << "  realtime_convolution:" << endl
         << "       Simulate running on an audio thread." << endl
         << "  file_test:" << endl
         << "       Run on an actual audio file." << endl
         << endl
         << "Remarks:" << endl
         << "  The default behaviour is to run all tests." << endl
         << endl;
}

int main(int argc, const char **argv)
{

    CommandLineParser parser;

    bool help = false;
    std::string testName;
    bool displaySectionPlans = false;

    try
    {
        parser.AddOption("h", "help", &help);
        parser.AddOption("", "short", &shortTests);
        parser.AddOption("", "profile", &profilerFileName);
        parser.AddOption("", "build", &buildTests);
        parser.AddOption("", "plans", &displaySectionPlans);

        parser.Parse(argc, argv);

        if (help)
        {
            PrintHelp();
            return EXIT_SUCCESS;
        }
        if (parser.Arguments().size() > 1)
        {
            throw CommandLineException("Incorrect number of parameters.");
        }
        if (parser.Arguments().size() == 1)
        {
            testName = parser.Arguments()[0];
        }
    }
    catch (const std::exception &e)
    {
        cout << "ERROR: " << e.what() << endl;
        return EXIT_FAILURE;
    }

#pragma GCC diagnostic ignored "-Wdangling-else" // stupid error.
    try
    {
        SetDisplaySectionPlans(displaySectionPlans);

        /*  ADD_TEST_NAME_HERE  (don't forget to revise PrintHelp()) )*/

        if (testName == "file_test")
        {
            TestFile();
        } else if (testName == "TestDirectConvolutionSection")
        {
            TestDirectConvolutionSection();
        }
        else if (testName == "sequencing")
        {
            TestBalancedConvolutionSequencing();
        }
        else if (testName == "check_for_stalls")
        {
            // check for read stalls. Run indefinitely.
            while (true)
            {
                TestBalancedConvolution();
            }
        }
        else if (testName == "realtime_convolution_cpu_use")
        {
            RealtimeConvolutionCpuUse();
        }
        else if (testName == "realtime_convolution")
        {
            TestRealtimeConvolution();
        }
        else if (testName == "section_benchmark")
        {
            BenchmarkFftConvolutionStep();
        }
        else if (testName == "convolution_benchmark")
        {
            BenchmarkBalancedConvolution();
        }
        else if (testName == "section_allocations")
        {
            TestDirectConvolutionSectionAllocations();
        }
        else if (testName == "")
        {
            TestFft();
        }
        else
        {
            throw CommandLineException(SS("Unrecognized test name: " + testName));
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "TEST FAILED: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}