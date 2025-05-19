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
#include "HoltersGraphicEq.hpp"
#include <iostream>
#include <iomanip>
#include "LsNumerics/LsMath.hpp"
#include <cassert>

using namespace std;
using namespace LsNumerics;
using namespace toob::holters_graphic_eq;

static double omegaToF(double sampleRate, double omega)
{
    return omega * sampleRate / (2 * M_PI);
}



static double GetFrequencyResponse(GraphicEq &eq, double freq) {
    size_t samples = eq.getSampleRate()*4;

    double phase = 0;
    double dPhase = 2*M_PI*freq/eq.getSampleRate();

    double inSum = 0;
    double outSum = 0;
    for (size_t i = 0; i < samples/2; ++i)
    {
        float v = std::cos(phase);
        phase += dPhase;
        if (phase >= 2*M_PI)
        {
            phase -= 2*M_PI;
        }

        float out;
        eq.process(&v,&out,1);
    }
    for (size_t i = 0; i < samples; ++i)
    {
        float v = std::cos(phase);
        phase += dPhase;
        if (phase >= 2*M_PI)
        {
            phase -= 2*M_PI;
        }

        float out;
        eq.process(&v,&out,1);

        inSum += v*v;
        outSum += out*out;
    }
    return std::sqrt(outSum/inSum);
}

void TestSetting(GraphicEq &eq, std::vector<double>&settings);

void TestBands()
{
    float sampleRate = 48000;
    size_t bands = 10;
    float fc0 = 30;
    float r = 2.0;
    GraphicEq eq(sampleRate, bands, fc0, r);
    cout << "Parameter check (bands=" << bands
         << " fc0=" << fc0
         << " r=" << r << ")" << endl;
    cout << "  compare against tables 1 and 2 in [1]" << endl;
    cout << endl;
    cout << setw(8) << "Band"
         << setw(8) << "fC"
         << setw(8) << "fL"
         << setw(8) << "fU"
         << setw(8) << "fM"
         << endl;
    for (size_t i = 0; i < bands; ++i)
    {
        auto &band = eq.bandFilters()[i];
        cout << setw(8) << (i + 1)
             << setw(8) << setprecision(6) << omegaToF(sampleRate, band->Omega_C)
             << setw(8) << setprecision(6) << omegaToF(sampleRate, band->Omega_L)
             << setw(8) << setprecision(6) << omegaToF(sampleRate, band->Omega_U)
             << setw(8) << setprecision(6) << omegaToF(sampleRate, band->Omega_M)
             << endl;
    }
    cout << endl;

    // table 2.
    for (size_t i = 0; i < bands; ++i)
    {
        eq.setGain(i, (i & 1) == 0 ? Db2Af(12) : Db2Af(-12));
    }

    cout << setw(8) << "i"
         << setw(8) << "K[i]" << setprecision(7) << endl;

    for (size_t i = 0; i < bands; ++i)
    {
        cout << setw(8) << (i + 1)
             << " " << setw(8) << setprecision(6) << eq.bandFilters()[i]->K << endl;
    }

    // test a few parameter values
    assert(f_compare(
        omegaToF(sampleRate, eq.bandFilters()[2]->Omega_U),
        170,
        1.0));

    assert(f_compare(
        omegaToF(sampleRate, eq.bandFilters()[7]->Omega_M),
        3861,
        1.0));

    // make sure none of the filter are OBVIOUSLY bad.

    for (size_t i = 0; i < 40000; ++i)
    {
        float in = 1.0f;
        float out;
        eq.process(&in,&out,1);
        assert(out < 100.0);
    }

    std::vector<double> expectedDb;
    expectedDb.resize(bands);
    cout << "Max bands." << endl;
    for (size_t i = 0; i < expectedDb.size(); ++i)
    {
        expectedDb[i] = 12;
    }

    TestSetting(eq,expectedDb);

    cout << "Alternating bands." << endl;
    for (size_t i = 0; i < expectedDb.size(); ++i)
    {
        expectedDb[i] = (i & 1) ? -12: 12;
    }

    TestSetting(eq,expectedDb);
}

void TestSetting(GraphicEq &eq, std::vector<double>&settings)
{
    auto & bands = eq.bandFilters();
    for (size_t i = 0; i < bands.size(); ++i)
    {
        eq.setGain(i,Db2Af(settings[i]));
    }


    for (size_t i = 0; i < bands.size(); ++i)
    {
        float freq = omegaToF(eq.getSampleRate(),bands[i]->Omega_C);
        float fL = omegaToF(eq.getSampleRate(),bands[i]->Omega_L);
        float fU = omegaToF(eq.getSampleRate(),bands[i]->Omega_U);

        double response = GetFrequencyResponse(eq,freq);
        double expectedDb = settings[i];

        double actualDb = Af2Db(response);
        cout << setprecision(7) << freq
            << "  Expected fM: " << expectedDb 
            << " dB  measured fM: " << actualDb 
            << " dB  H_l: " << GetFrequencyResponse(eq,fL) 
            << " dB  H_u: " << GetFrequencyResponse(eq,fU)
            << " dB" << endl;

        // assert(f_compare(expectedDb,actualDb,1.0));
    }
}


void TestBand()
{
    float sampleRate = 48000;
    size_t bands = 1;
    float fc0 = 240;
    float r = 2.0;
    float gain = 6;
    size_t TEST_BAND = 0;

    GraphicEq eq(sampleRate, bands, fc0, r);

    float lf = omegaToF(eq.getSampleRate(),eq.bandFilters()[TEST_BAND]->Omega_L);
    float hf = omegaToF(eq.getSampleRate(),eq.bandFilters()[TEST_BAND]->Omega_U);

    cout << "Test a single band, gain=" << gain << endl;
    cout << "-------------------------" << gain << endl;
    cout << " fM=" << omegaToF(eq.getSampleRate(),eq.bandFilters()[TEST_BAND]->Omega_M);
    cout << " fC=" << omegaToF(eq.getSampleRate(),eq.bandFilters()[TEST_BAND]->Omega_C);
    cout << " fL=" << lf
    << " fU=" << hf
    << endl;


    for (size_t i = 0; i < bands; ++i)
    {
        eq.setGain(i,1.0);
    }
    eq.setGain(TEST_BAND,Db2Af(gain));


    for (int i = -10; i < 20; ++i)
    {
        float blend = i/10.0;
        float freq = (1-blend)*lf+blend*hf;

        double response = GetFrequencyResponse(eq,freq);

        double actualDb = Af2Db(response);
        cout << "   " << setprecision(7) << freq
            << " measured(dB): " << actualDb 
            << " expected: " << Af2Db(eq.getFrequencyResponse(freq))
            << endl;

        // assert(f_compare(expectedDb,actualDb,1.0));
        freq *= r;
    }
    cout << endl;


}
int main(int argc, char **argv)
{
    TestBand();
    TestBands();
    return EXIT_SUCCESS;
}