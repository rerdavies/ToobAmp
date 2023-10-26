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

#include "CombFilter2.h"
#include <stdexcept>
#include <vector>
#include <iostream>

using namespace std;


void require(bool testValue)
{
    if (!testValue) 
    {
        //cout << "Test failed.";
        throw std::runtime_error("Test failed.");
    }
}

using namespace toob;


double GetResponse(double sampleRate, double combFrequency, double signalFrequency)
{
    CombFilter combFilter;
    combFilter.SetSampleRate(sampleRate);
    combFilter.UpdateFilter(combFrequency,1);

    size_t i = 0;
    double dx = 2*std::numbers::pi*signalFrequency/sampleRate;
    double x = i*dx;
    double lastResult = 1;

    // prime the fir filter
    while (true)
    {
        x = dx*i++;
        double y = std::sin(x);
        double result = combFilter.Tick(y);
        if (lastResult < 0 && result >= 0 && i >= CombFilter::FIR_LENGTH*4)
        {
            break;
        }
        lastResult = result;
    }

    // calculate response.
    size_t nSamples = 0;
    (void)nSamples;
    double inputSum = 0;
    double sum = 0;

    while (true)
    {
        x = dx*i++;
        double y = std::sin(x);
        inputSum += std::abs(y);
        double result = combFilter.Tick(y);
        sum += std::abs(result);
        ++nSamples;
        if (lastResult < 0 && result >= 0 && i >= sampleRate*3 + CombFilter::FIR_LENGTH*4)
        {
            break;
        }
        lastResult = result;
    }

    double response = sum/inputSum;
    return response;


}
void FrequencyPlot(double sampleRate, double combFrequency)
{
    CombFilter combFilter;
    combFilter.SetSampleRate(sampleRate);
    combFilter.UpdateFilter(combFrequency,1);

    constexpr size_t N_SAMPLES = 100;
    double logMin = std::log(100);
    double logMax = std::log(sampleRate/2);

    cout << "Frequency Plot: sr=" << sampleRate << " f0=" << combFrequency << endl;
    for (size_t i = 0; i < N_SAMPLES; ++i)
    {
        double f = std::exp(logMin+ i*(logMax-logMin)/N_SAMPLES);
        double expected = combFilter.GetFrequencyResponse(f);
        double actual = GetResponse(sampleRate,combFrequency,f);
        cout << f << ',' << actual << ',' << expected << endl;

    }
}

void FrequencyResponseTest(double sampleRate, double combFrequency, double signalFrequency)
{
    double response = GetResponse(sampleRate,combFrequency,signalFrequency);
    CombFilter combFilter;
    combFilter.SetSampleRate(sampleRate);
    combFilter.UpdateFilter(combFrequency,1);
    double expected = combFilter.GetFrequencyResponse(signalFrequency);

    double e = std::abs(response-expected);
    if ( e > 1E-2)
    {
        cout << "Failed: sr=" << sampleRate << " f0=" << combFrequency << " f=" << signalFrequency << " e=" << e << endl;
    }
//    require (e < 1E-2);
}
void FrequencyResponseTest()
{
    for (double sampleRate : std::vector<double>{41000, 44000, 44000 * 2, 44000 * 4})
    {
        constexpr size_t N_SAMPLES = 25;
        double logMin = std::log(100);
        double logMax = std::log(19000);

        for (size_t ifc = 0; ifc < N_SAMPLES; ++ifc)
        {
            double combFrequency = std::exp(logMin+ ifc*(logMax-logMin)/N_SAMPLES);

            for (size_t ifs = 0; ifs < N_SAMPLES; ++ifs)
            {
                double signalFrequency = std::exp(logMin+ ifs*(logMax-logMin)/N_SAMPLES);

                FrequencyResponseTest(sampleRate,combFrequency,signalFrequency);
            }
        }
    }


}

int main(void)
{
    // FrequencyPlot(44000,1410.01);

    FrequencyResponseTest();
    return 0;
}