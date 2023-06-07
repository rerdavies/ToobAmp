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

#include "Ce2Chorus.hpp"
#include "Tf2Flanger.hpp"
#include "Filters/ShelvingLowCutFilter2.h"


#include <iostream>
#include <fstream>
#include <filesystem>
#include <limits>

using namespace toob;
using namespace std;

void TestChorus()
{
    Ce2Chorus chorus;
    double sampleRate = 48000;
    chorus.SetSampleRate(sampleRate);
    Ce2Chorus::Instrumentation chorusTest(&chorus);

    chorus.SetDepth(0.5);
    chorus.SetRate(1);

    std::filesystem::path path = std::filesystem::path("/tmp/chorusTest.tsv");

    std::ofstream f;
    f.open(path);

    for (int i = 0; i < 500; ++i)
    {
        for (int j = 0; j < 100 - 1; ++j)
        {
            chorusTest.TickLfo();
        }
        f << chorusTest.TickLfo() << endl;
    }

    // /* Data around the first maximum */
    // for (int i = 0; i < 3*sampleRate/3.25/4-50; ++i)
    // {
    //     chorusTest.TickLfo();
    // }
    // for (int i = 0; i < 600; ++i)
    // {
    //     f << chorusTest.TickLfo() << endl;
    // }
}

struct FlangerTestResult
{
    float minDelay = -1;
    float maxDelay = -1;
    float rate = -1;
};

std::ostream &operator<<(std::ostream &s, const FlangerTestResult &testResult)
{
    s << "minDelay=" << testResult.minDelay << " maxDelay=" << testResult.maxDelay << " f=" << testResult.rate << endl;
    return s;
}

FlangerTestResult TestFlangerExcursion(float manual, float depth, float rate, float res)
{
    Tf2Flanger flanger;
    flanger.SetSampleRate(48000);
    Tf2Flanger::Instrumentation flangerTest(&flanger);
    flanger.SetManual(manual);
    flanger.SetDepth(depth);
    flanger.SetRate(rate);
    flanger.SetRes(res);


    float vMin = std::numeric_limits<float>::max();
    float vMax = -std::numeric_limits<float>::max();
    float vLast = 0;
    bool increasing = false;
    bool lastIncreasing = false;
    int64_t tFirstInflection = -1;
    int64_t tLastInflection = -1;
    int64_t nInflections = 0;
    for (int64_t i = 0; i < 48000; ++i)
    {
        float v = flangerTest.TickLfo();

        if (v != vLast)
        {
            lastIncreasing = increasing;
            increasing = v > vLast;
            vLast = v;
        }
    }

    for (int64_t i = 0; i < 48000 * 16; ++i)
    {
        float v = flangerTest.TickLfo();
        if (v > vMax)
            vMax = v;
        if (v < vMin)
            vMin = v;

        if (v != vLast)
        {
            lastIncreasing = increasing;
            increasing = v > vLast;
            vLast = v;
            if (increasing != lastIncreasing)
            {
                if (tFirstInflection == -1)
                {
                    tFirstInflection = i;
                }
                tLastInflection = i;
                nInflections++;
            }
        }
    }

    return FlangerTestResult{vMin, vMax, 2 * (tLastInflection - tFirstInflection) * 1.0f / nInflections / 48000};
}

void TestFlanger()
{

    {
        FlangerTestResult result = TestFlangerExcursion(0.5, 0.5, 0.5, 0.5);
        cout << "Default: " << result << endl;
    }

    Tf2Flanger flanger;
    double sampleRate = 48000;
    flanger.SetSampleRate(sampleRate);
    Tf2Flanger::Instrumentation flangerTest(&flanger);

    flanger.SetManual(0);
    flanger.SetDepth(0.5);
    flanger.SetRate(1);
    flanger.SetRes(0.5);
    flanger.Clear();

    std::filesystem::path path = std::filesystem::path("/tmp/flangerTest.tsv");

    std::ofstream f;
    f.open(path);

    for (int i = 0; i < 48000; ++i)
    {
        for (int j = 0; j < 100 - 1; ++j)
        {
            flangerTest.TickLfo();
        }
    }

    for (int i = 0; i < 48000 / 4; ++i)
    {
        for (int j = 0; j < 100 - 1; ++j)
        {
            flangerTest.TickLfo();
        }
        f << flangerTest.TickLfo() << endl;
    }

    using namespace std;

    {
        FlangerTestResult result = TestFlangerExcursion(0.0, 0.0, 0.0, 0.5);
        cout << "Min Manual: " << result << endl;
    }
    {
        FlangerTestResult result = TestFlangerExcursion(1.0, 0.0, 1.0, 0.5);
        cout << "Max Manual: " << result << endl;
    }
    {
        FlangerTestResult result = TestFlangerExcursion(0.5, 0.5, 0.5, 0.5);
        cout << "Default: " << result << endl;
    }

    // /* Data around the first maximum */
    // for (int i = 0; i < 3*sampleRate/3.25/4-50; ++i)
    // {
    //     flangerTest.TickLfo();
    // }
    // for (int i = 0; i < 600; ++i)
    // {
    //     f << flangerTest.TickLfo() << endl;
    // }
}


void TestFilter()
{
    using namespace std;
    toob::ShelvingLowCutFilter2 lowShelf;
    toob::ShelvingLowCutFilter2 highShelf;

    lowShelf.SetSampleRate(48000);
    highShelf.SetSampleRate(48000);
    lowShelf.Design(0,15,1000);
    highShelf.Design(0,-15,1000);

    ofstream f("/tmp/FilterRespose.txt");

    for (float fC = 25; fC < 22000; fC *= 1.14f)
    {

        f << setw(12) << right << fC
            << setw(12) << right << LsNumerics::Af2Db(lowShelf.GetFrequencyResponse(fC))
            << setw(12) << right << LsNumerics::Af2Db(highShelf.GetFrequencyResponse(fC))
            << setw(12) << right << LsNumerics::Af2Db(lowShelf.GetFrequencyResponse(fC)*highShelf.GetFrequencyResponse(fC))
            << endl;
    }


    // stability test
    {
        toob::ShelvingLowCutFilter2 filter;
        filter.SetSampleRate(48000);
        filter.Design(0,-15,1000);
        filter.Tick(1);

        for (size_t i = 0; i < 48000*10; ++i)
        {
            float value = filter.Tick(i & 1);
            if (std::abs(value) > 10)
            {
                throw std::logic_error("Filter not stable!");
            }
        }
    }


    for (int64_t i = 0; i < 48000*10; ++i)
    {

    }
}

int main(int argc, char **argv)
{
    TestFilter();

    // TestFlanger();
    // TestChorus();
    return 0;
}
