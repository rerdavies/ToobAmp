/*
 Copyright (c) 2023 Robin Davies

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "MotorolaResampler.hpp"
#include <cmath>
#include <numbers>
#include "../AudioData.hpp"
#include "../WavWriter.hpp"
#include <stdexcept>
#include <cmath>
#include "LsMath.hpp"
#include <iostream>
#include <functional>
#include "Window.hpp"
#include <iomanip>
#include <chrono>

using namespace toob;
using namespace std;
using namespace LsNumerics;
using namespace std::chrono;

std::vector<double> MakeFilter(size_t inputRate, size_t outputRate)
{
    std::vector<double> filter;
    double filterRatio;
    if (outputRate > inputRate)
    {
        // straightforware nyquist filter.
        filterRatio = 0.5;
    }
    else
    {
        double cutoff = 20000.0 * outputRate / 44100;
        filterRatio = cutoff / inputRate;
    }

    constexpr int64_t filterSize = 200;
    filter.resize(filterSize);

    for (int64_t i = 0; i < filterSize; ++i)
    {
        double k = i - (filterSize / 2);
        double x = k * filterRatio * std::numbers::pi * 2;
        if (x == 0)
        {
            filter[i] = 1;
        }
        else
        {
            filter[i] = sin(x) / x;
        }
    }
    auto window = LsNumerics::Window::Hamming<double>(filter.size());
    for (size_t i = 0; i < filter.size(); ++i)
    {
        filter[i] *= window[i];
    }
    return filter;
}

static size_t Gcd(size_t a, size_t b)
{
    while (true)
    {
        if (a == 0)
            return b;
        if (b == 0)
            return a;
        size_t q = a / b;
        size_t r = a - q * b;
        // return Gcd(b,r);
        a = b;
        b = r;
    }
}

std::vector<float> PolyphaseResample(size_t inputRate, size_t outputRate, const std::vector<float> &source)
{
    std::vector<double> filter;
    filter = MakeFilter(inputRate, outputRate);

    std::vector<float> output;
    size_t gcd = Gcd(inputRate, outputRate);
    inputRate /= gcd;
    outputRate /= gcd;
    upfirdn((int)inputRate, (int)outputRate, source, filter, output);
    return output;
}

size_t findHalfSampleOffset(size_t num, size_t denom)
{
    // ix * 44100/48000 = int+ 0.5; solve for ix.
    for (size_t i = 300; i < 0x800000; ++i)
    {
        // ix * 44100/48000 = int+ 0.5; solve for ix.
        // ix * 44100 = (int + 0.5) *48000
        // ix * 44100*2 = (2int + 1)* 48000;
        // 0 = (2int+1)*48000 % 44100
        size_t x = (2 * i + 1) * denom % (num * 2);
        if (x == 0)
        {
            size_t ix = (2 * i + 1) * denom / (num * 2);
            double check = ix * num / (double)denom;
            if (std::abs(check - (int64_t)check) != 0.5)
            {
                throw std::logic_error("Check failed.");
            }
            return ix;
        }
        double t = i * num / (double)denom;
        if (std::abs(t - (int64_t)t) == 0.5)
        {
            return i;
        }
    }
    throw std::logic_error("Can't find value.");
}

void WriteImpulseResponse()
{
    std::vector<float> input;

    input.resize(2000);
    // ix * 48000/44100 = int+ 0.5;
    // gcd(48000,44100) = 300
    // ix * 160/147 = int + 0.5
    // ix * 160 = (int +0.5)*147
    // ix = (int +0.5)*147/160
    // ix*160 = (int+0.5)*147
    // 0 % 160 = (int + 0.5)*147 % 160
    // lcm(147,160) = 23520
    // factors of 147: 3 x 7^2
    // factors 160: 2^5 x 5

    const size_t halfSampleOffset = findHalfSampleOffset(44100, 48000);
    (void)halfSampleOffset;
    input[100] = 1;

    AudioData audioData(44100, input);

    audioData.Resample(48000);
    WavWriter writer("/tmp/test.wav");
    writer.Write(audioData, false);
}

using ResampleFunction = std::function<
    std::vector<float>(
        int64_t fromFrequency,
        int64_t toFrequency,
        std::vector<float> &input)>;

std::vector<float> AudioDataResample(int64_t fromFrequency, int64_t toFrequency, const std::vector<float> &input)
{
    AudioData audioData(fromFrequency, input);
    audioData.Resample(toFrequency);
    return audioData.getChannel(0);
}


double Rms(const std::vector<float>&buffer)
{
    double sum = 0;
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        sum += buffer[i]*buffer[i];
    }
    return std::sqrt(sum/buffer.size());
}

float GetFrequencyResponse(int64_t fromFrequency, int64_t toFrequency, double frequency, const ResampleFunction &resampleFn)
{
    std::vector<float> input;
    input.resize(40000);

    double m;
    m = frequency * (std::numbers::pi * 2) / fromFrequency;

    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = std::cos(i * m);
    }
    std::vector<float> channel = resampleFn(fromFrequency, toFrequency, input);

    double gain = Rms(channel)/Rms(input);
    float db = LsNumerics::Af2Db((float)gain);
    return db;
}

void CheckFrequencyResponse(
    int64_t fromFrequency,
    int64_t toFrequency,
    const ResampleFunction &resampleFn)
{
    float cutoffFrequency;
    if (fromFrequency < toFrequency)
    {
        cutoffFrequency = fromFrequency / 2;
    }
    else
    {
        cutoffFrequency = 20000.0 * toFrequency / 44100;
    }
    double maxFrequency = std::min(fromFrequency, toFrequency) / 2;
    if (fromFrequency > toFrequency)
    {
        maxFrequency = toFrequency/2 + (toFrequency/2-cutoffFrequency);
    }
    double passbandRippleMax = -1E6;
    double passbandRippleMin = 1E6;
    cout << setw(16) << left << "Freq" << setw(16) << left << "Atten" << endl;

    for (float frequency = 0; frequency < maxFrequency; frequency += toFrequency / 521.0)
    {
        float db = GetFrequencyResponse(fromFrequency, toFrequency, frequency, resampleFn);

        cout << setw(16) << right << frequency << setw(16) << right << db << endl;
        if (frequency < cutoffFrequency)
        {
            if (db > passbandRippleMax)
            {
                passbandRippleMax = db;
            }
            if (db < passbandRippleMin)
            {
                passbandRippleMin = db;
            }
        }
        else
        {
            if (db > 0.05)
            {
                // throw std::logic_error("Frequency response failed.");
            }
        }
    }
    
    float dbMaxF = GetFrequencyResponse(fromFrequency, toFrequency, maxFrequency, resampleFn);

    cout << "    from: " << fromFrequency << "hz to: " << toFrequency
         << "hz Passband Ripple: " << passbandRippleMin << " to " << passbandRippleMax
         << " Response at " << maxFrequency << "hz: " << dbMaxF;

    if (fromFrequency > toFrequency)
    {
        float rejectFrequency = toFrequency / 2 + (toFrequency / 2 - 20000);
        float dbReject = GetFrequencyResponse(fromFrequency, toFrequency, rejectFrequency, resampleFn);
        cout << " Response at " << rejectFrequency << "hz: " << dbReject;
    }

    cout << endl;

    // time a 4 second resample.
 
    std::vector<float> input;
    input.resize(fromFrequency*4);
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
    {
        resampleFn(fromFrequency,toFrequency,input);
    }
    auto elapsed = Clock::now()-start;

    auto t = duration_cast<milliseconds>(elapsed);
    cout << "    Time to resample 4s sample: " << setprecision(3) << (t.count()/1000.0) << endl;

    if (passbandRippleMax > 3 || passbandRippleMin < -3)
    {
        throw std::logic_error("Frequency response test failed.");
    }
}

void ResamplerTest()
{

    CheckFrequencyResponse(96000, 44100, AudioDataResample);

    cout << "=== ResamplerTest ===" << endl;
    WriteImpulseResponse();

    cout << "   --- Polyphase Filter resampling" << endl;

    CheckFrequencyResponse(96000, 48000, PolyphaseResample);
    CheckFrequencyResponse(96000, 44100, PolyphaseResample);

    cout << "   --- AudioData resampling" << endl;

    CheckFrequencyResponse(96000, 48000, AudioDataResample);
    CheckFrequencyResponse(96000, 44100, AudioDataResample);
    CheckFrequencyResponse(48000, 44100, AudioDataResample);
    CheckFrequencyResponse(44100, 48000, AudioDataResample);
}

int main(int argc, char **argv)
{
    ResamplerTest();
}