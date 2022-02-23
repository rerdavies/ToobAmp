/*
 *   Copyright (c) 2021 Robin E. R. Davies
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

#include "PitchDetector.hpp"
#include <fstream>
#include <limits>
#include "Window.hpp"

using namespace LsNumerics;


static constexpr double GUITAR_LOW_E_FREQUENCY = 82.41; // hz.
static constexpr double MAXIMUM_DETECTABLE_FREQUENCY = 923.33; // gutar, high E, 19th fret.
static constexpr double MINIMUM_DETECTABLE_FREQUENCY = 75; // guitar low E - 1/2 semi-tone.
static constexpr double OCTAVE_THRESHOLD = 0.5;

static constexpr int WINDOW_PERIODS_REQUIRED = 8;


PitchDetector::PitchDetector(){

}

PitchDetector::PitchDetector(int sampleRate, int fftSize) {
    Initialize(sampleRate,fftSize);
}
void PitchDetector::Initialize(int sampleRate, int fftSize) {
    this->sampleRate = sampleRate;
    this->fftSize = fftSize;
    this->fft.SetSize(fftSize);
    this->paddedFft.SetSize(fftSize*2);

    this->window = Window::ExactBlackman<double>(fftSize);
    allocateBuffers();
    this->minimumCepstrumBin = (int)(sampleRate/MAXIMUM_DETECTABLE_FREQUENCY);
    this->maximumCepstrumBin = (int)(sampleRate/MINIMUM_DETECTABLE_FREQUENCY);
}

PitchDetector::PitchDetector(int sampleRate)
{
    Initialize(sampleRate);
}

void PitchDetector::Initialize(int sampleRate)
{
    Initialize(sampleRate, LsNumerics::NextPowerOfTwo(
            // FFT_SIZE = SAMPLERATE/MINIMUM_DETECTABLE_FREQUENCY*HAMMING_WINDOW_PERIODS_REQUIRED 
            std::max(
                // FFT_SIZE = SAMPLERATE/MINIMUM_DETECTABLE_FREQUENCY*HAMMING_WINDOW_PERIODS_REQUIRED 
                (double)sampleRate /MINIMUM_DETECTABLE_FREQUENCY*WINDOW_PERIODS_REQUIRED,
                MAXIMUM_DETECTABLE_FREQUENCY*2
            )

    ));
}





void PitchDetector::allocateBuffers() {
    this->conversionBuffer.resize(fftSize);
    this->scratch.resize(fftSize*2);
    this->scratch2.resize(fftSize*2);
    this->cepstrum.resize(fftSize/2);

    this->autoCorrelation.resize(fftSize);
    speciallyNormalizedAutoCorrelation.resize(fftSize/2);

}


bool PitchDetector::findCubicMaximum(int binNumber, std::vector<double>& x, CubicResult &result) {
    // Find the peak of a cubic interpolation of 3 values around the peak
    // to find a more precise maximum. (FFT lore).
    // We're really interesetd in the X value. But the Y value may be useful when comparing peaks.
    double p0 = x[binNumber - 1];
    double p1 = x[binNumber];
    double p2 = x[binNumber + 1];
    p0 = std::log(std::max(1E-300, p0));
    p1 = std::log(std::max(1E-300, p1));
    p2 = std::log(std::max(1E-300, p2));

    if (std::abs(p0 - p1) < 1E-7 && std::abs(p1 - p2) < 1E-7) return false;
    // f(x) = ax^x+bx+c
    // f(-1) = a-b+c = p0
    // f(0) = c = p1
    // f(1) = a+b+c = p2;
    double c = p1;
    // 2a + 2c = (p0+p2)
    // a = (p0+p2)/2-c;
    double a = (p0 + p2) / 2 - c;
    double b = p2 - a - c;
    // max is at f'(x) = 0 (there always is one.
    // f'(x) = 2ax+b = 0
    // 2ax = -b
    // x = -b/2a
    double xResult = -b / (2 * a);
    result.x = binNumber + xResult;
    result.y = std::exp(a * xResult * xResult + b * xResult + c);
    return true;
}


inline double PitchDetector::findCepstrumFrequency(std::vector<double>& cepstrum) {
    double dx = 1.0/fftSize;
    size_t n = cepstrum.size();
    double bestX = -1;
    double bestValue = -std::numeric_limits<double>::max();
    bool peaked = false;

    for (int i = 1; i < cepstrum.size(); ++i) {
        double currentValue = cepstrum[i];
        if (currentValue >cepstrum[i-1] && currentValue > cepstrum[i+1])
        {
            if (currentValue > bestValue)
            {
                peaked = true;
                bestValue = currentValue;
                bestX = i;
            } 
        }
        if (peaked && currentValue < bestValue*0.5)
        {
            // while in the same run of high values, anything better is better.
            // But we are well past the peak, so the next peak must be MUCH better than this one.
            // The alternative is to do cubic evaluation to avoid quantization noise around the each
            // candidate peak.
            bestValue *= 1.2; 
            peaked = false;
        }
    }
    if (bestX == -1)
    {
        return 0;
    }

    CubicResult cepstrumResult;
    if (findCubicMaximum(bestX,cepstrum,cepstrumResult))
    {
        return this->sampleRate/cepstrumResult.x;
    }
    return 0;
}

double PitchDetector::detectPitch(std::vector<complex> & signal) {
    // autoCorrelation = cross-correlate(signal^2,window)
    
    // Cepstrum.
    for (int i = 0; i < fftSize; ++i) {
        scratch[i] = window[i] * signal[i]*signal[i]; // non-linearity reduces cepstrum octave errors ;
    }
    fft.forward(scratch,scratch2);

    for (int i = 0; i < fftSize; ++i)
    {
        complex t = scratch2[i];

        scratch2[i] = std::abs(t);
    }

    fft.forward(scratch2,scratch);

    for (size_t i= 0; i < cepstrum.size(); ++i)
    {
        complex t = scratch[i];
        cepstrum[i] = std::abs(t);
    }

#if 0
    {
        std::ofstream f;
        f.open("/home/rerdavies/temp/data.tsv");
        assert(!f.fail());
        for (int i = 0; i < cepstrum.size(); ++i)
        {
            double v = cepstrum[i];
            f << v << '\n';
        }
        f << '\n';

    }
#endif 
#if 0
    {
        std::ofstream f;
        f.open("/home/rerdavies/temp/data.tsv");
        assert(!f.fail());
        f << "FFT\tCEPS" << std::endl;
        for (int i = 0; i < cepstrum.size(); ++i)
        {
            double v = cepstrum[i];
            double v2 = (scratch2[i]*std::conj(scratch2[i])).real();
            f << v2 << "\t" << v <<  std::endl;
        }
        f << '\n';

    }
#endif


    double cepstrumFrequency = findCepstrumFrequency(cepstrum);
    return cepstrumFrequency;
}

