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

#include "Phaser.hpp"
#include "CommandLineParser.hpp"
#include <iostream>
#include "LsNumerics/Fft.hpp"
#include "LsNumerics/StagedFft.hpp"
#include <vector>
#include <sstream>
#include <cassert>


using namespace toob;
using namespace std;
using namespace LsNumerics;

static std::string Bar(double value) {
    std::stringstream ss;
    for (size_t i = 0; i < 20; ++i)
    {
        ss << ((value > i*0.1) ? "O": " ");
    }
    return ss.str();
}
static double ResponseAt(Phaser &phaser, double frequency) {
    double rate = phaser.getSampleRate();

    double dPhase = 2*M_PI*frequency/ rate;

    double phase = 0;

    size_t sampleSize = (size_t)(rate*20);

    for (size_t i = 0; i < sampleSize; ++i) {
        phase += dPhase;
        if (phase >= 2*M_PI) {
            phase -= 2*M_PI;
        }
        double v = std::sin(phase);
        phaser.process(v);
    }
    double sumIn = 0;
    double sumOut = 0;

    for (size_t i = 0; i < sampleSize; ++i) {
        phase += dPhase;
        if (phase >= 2*M_PI) {
            phase -= 2*M_PI;
        }
        double v = std::sin(phase);
        double out = phaser.process(v);
        sumIn += v*v;
        sumOut += out*out;
    }
    return sumOut/sumIn;

}
void GenerateFrequencyResponse(double f)
{
    float sampleRate = 48000;
    Phaser phaser{sampleRate};
    phaser.testSetLfoPosition(0);

    const size_t FFT_SIZE = 4*1024;
    
    StagedFft fft(FFT_SIZE);

    std::vector<float> inputBuffer;
    inputBuffer.resize(FFT_SIZE);
    inputBuffer[0] = 1.0f;
    std::vector<float> outputBuffer;
    outputBuffer.resize(FFT_SIZE);

    for (size_t i = 0; i < FFT_SIZE; ++i)
    {
        outputBuffer[i] = phaser.process(inputBuffer[i]);
    }

    std::vector<std::complex<double>> fftIn;
    fftIn.resize(FFT_SIZE);

    std::vector<std::complex<double>> fftOut;
    fftOut.resize(FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE; ++i) 
    {
        fftIn[i] = outputBuffer[i];
    }
    fft.Forward(fftIn,fftOut);

    double scale = std::sqrt((double)FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE/2; ++i)
    {
        float f = i*sampleRate/(FFT_SIZE);
        if (f > 2000) break;

        cout << f << ", " << Bar(scale*std::abs(fftOut[i])) << endl;
    }
    cout << endl;
}

static void TestFrequencyResponse(float sampleRate, float lfoPosition)
{
    cout << "TestFrequencyResponse(" << sampleRate << ", " << lfoPosition << ")" << endl;
    Phaser phaser(sampleRate);
    phaser.testSetLfoPosition(lfoPosition);

    Phase90Lfo lfo(sampleRate);
    float freq = lfo.lfoToFreq(lfoPosition);

    // locate the frequency of the second notcch.
    float fMultiplierLow = tan(M_PI/8.0);
    float fMultiplierHigh = tan(M_PI*3.0/8.0);
    float secondaryFrequency = freq * fMultiplierHigh/fMultiplierLow;

    cout << "Sample Rate: " << sampleRate << " lfo: " << lfoPosition << endl;
    cout << "     f1: " << freq << "Hz" << " reponse: " << ResponseAt(phaser,freq) << endl;
    cout << "     f2: " << secondaryFrequency << "Hz" << " reponse: " << ResponseAt(phaser,secondaryFrequency) << endl;

    assert(ResponseAt(phaser,freq) < 1E-5);
    assert(ResponseAt(phaser,secondaryFrequency) < 1E-7);

}

void f_assert(float v1, float v2) 
{
    if (std::abs(v1-v2) > 1E-3) {
        std::stringstream ss;
        ss << v1 << "!=" << v2 << "  e=" << v2-v1;
        throw std::runtime_error(ss.str());
    }
}
static void TestFrequencyResponse(float sampleRate)
{
    Phase90Lfo lfo(sampleRate);
    cout << "k: "  << Phase90Lfo::k << " a: " << Phase90Lfo::a << " m: " << Phase90Lfo::m << " c: " << Phase90Lfo::c << endl;

    cout << "V(0) = " << lfo.lfoToFreq(0) << endl;
    cout << "V(0.5) = " << lfo.lfoToFreq(0.5) << endl;
    cout << "V(1.0) = " << lfo.lfoToFreq(1.0) << endl;

    f_assert(lfo.lfoToFreq(0), Phase90Lfo::VLO);
    f_assert(lfo.lfoToFreq(0.5) , Phase90Lfo::VMID);
    f_assert(lfo.lfoToFreq(1.0) ,Phase90Lfo::VHI);
    

    TestFrequencyResponse(sampleRate,0);
    TestFrequencyResponse(sampleRate,0.5);
    TestFrequencyResponse(sampleRate,1.0);
}

void TestFrequencyResponse() {
    // Check for existence of two phaser filter notches at the correct frequency..

    TestFrequencyResponse(44100);
    TestFrequencyResponse(48000);
    TestFrequencyResponse(96000);
    cout << "Test passed." << endl;

}

int main(int argc, char **argv)
{
    try
    {
        CommandLineParser cmdline;
        double frequencyResponse = 0.0;

        cmdline.AddOption("f", "freq", &frequencyResponse);
        cmdline.Parse(argc, argv);

        if (frequencyResponse != 0)
        {
            GenerateFrequencyResponse(frequencyResponse);

            return EXIT_SUCCESS;
        } else {
            TestFrequencyResponse();
            return EXIT_SUCCESS;
        }
    }
    catch (const std::exception &e)
    {
        cerr << "ERROR: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}