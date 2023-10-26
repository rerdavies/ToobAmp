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

#include "PitchDetector.hpp"
#include <filesystem>
#include <fstream>
#include <limits>
#include "Window.hpp"
#include <exception>

using namespace LsNumerics;

//static constexpr double GUITAR_LOW_E_FREQUENCY = 82.41;        // hz.
static constexpr double MAXIMUM_DETECTABLE_FREQUENCY = 923.33; // gutar, high E, 19th fret.
static constexpr double MINIMUM_DETECTABLE_FREQUENCY = 55;     // guitar low E - ~5th.
//static constexpr double OCTAVE_THRESHOLD = 0.5;

static constexpr int WINDOW_PERIODS_REQUIRED = 4;

static inline double abs2(const std::complex<double> &c)
{
    return c.real() * c.real() + c.imag() * c.imag();
}


PitchDetector::PitchDetector()
{
}

PitchDetector::PitchDetector(int sampleRate, int fftSize)
{
    Initialize(sampleRate, fftSize);
}
void PitchDetector::Initialize(int sampleRate, int fftSize)
{
    this->sampleRate = sampleRate;
    this->cepstrumFftSize = fftSize;
    this->fftPlan.SetSize(fftSize);

#if LS_ENABLE_AUTO_CORRELATION_CODE
    this->crossCorrelationSize = this->cepstrumFftSize;
    this->crossCorrelationSamples = crossCorrelationSize / 2;
    this->crossCorrelationFft.SetSize(crossCorrelationSize);
#endif

    this->window = Window::Hann<double>(fftSize); // Grandke interpolation REQUIRES a Hann window.

    allocateBuffers();
    // f = this->sampleRate/(cepstrumIndex)
    this->minimumCepstrumBin = (int)(sampleRate / MAXIMUM_DETECTABLE_FREQUENCY/2);
    this->maximumCepstrumBin = (int)(sampleRate / MINIMUM_DETECTABLE_FREQUENCY)*3/2;

    // have to start scanning a bit earlier in order to detect  initial cepstrum minimum.
    this->minimumCepstrumBin = this->minimumCepstrumBin/3*2;

    // frequencyAdustmentFactor = calculateFrequencyAdjustmentFactor();
}

PitchDetector::PitchDetector(int sampleRate)
{
    Initialize(sampleRate);
}

void PitchDetector::Initialize(int sampleRate)
{
    // based on analytical results by Julius O. Smith. 
    // Adjusted empirically using test data including signal noise.
    Initialize(sampleRate, LsNumerics::NextPowerOfTwo(
                               2*std::max(
                                   (double)sampleRate / MINIMUM_DETECTABLE_FREQUENCY * WINDOW_PERIODS_REQUIRED,
                                   MAXIMUM_DETECTABLE_FREQUENCY * 2)

                                   ));
}

void PitchDetector::allocateBuffers()
{
#if LS_ENABLE_AUTO_CORRELATION_CODE
    size_t scratchSize = std::max(cepstrumFftSize, crossCorrelationSize);
#else
    size_t scratchSize = cepstrumFftSize;
#endif
    this->conversionBuffer.resize(scratchSize);
    this->scratchBuffer.resize(scratchSize);
    this->fftBuffer.resize(scratchSize);
    this->cepstrumBuffer.resize(scratchSize);
    this->cepstrum.resize(cepstrumFftSize / 2);

#if LS_ENABLE_AUTO_CORRELATION_CODE
    this->autoCorrelation.resize(crossCorrelationSize);
    speciallyNormalizedAutoCorrelation.resize(crossCorrelationSize / 2);
#endif
}

inline bool PitchDetector::findQuadraticMaximum(int binNumber, double p0, double p1, double p2, QuadResult &result)
{

    if (std::abs(p0 - p1) < 1E-7 && std::abs(p1 - p2) < 1E-7)
        return false;
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
    result.y = exp(a * xResult * xResult + b * xResult + c);
    return true;
}
inline bool PitchDetector::findQuadraticMaximum(int binNumber, std::vector<double> &x, QuadResult &result)
{
    // Find the peak of a quadratic interpolation of 3 values around the peak
    // to find a more precise maximum. (FFT lore).
    // We're really interesetd in the X value. But the Y value may be useful when comparing peaks.
    double p0 = x[binNumber - 1];
    double p1 = x[binNumber];
    double p2 = x[binNumber + 1];

    p0 = log(std::max(1E-300, p0));
    p1 = log(std::max(1E-300, p1));
    p2 = log(std::max(1E-300, p2));

    return findQuadraticMaximum(binNumber, p0, p1, p2, result);
}

bool PitchDetector::findQuadraticMaximumNoLog(int binNumber, std::vector<double> &x, QuadResult &result)
{
    // Find the peak of a quadratic interpolation of 3 values around the peak
    // to find a more precise maximum. (FFT lore).
    // We're really interesetd in the X value. But the Y value may be useful when comparing peaks.
    double p0 = x[binNumber - 1];
    double p1 = x[binNumber];
    double p2 = x[binNumber + 1];

    return findQuadraticMaximum(binNumber, p0, p1, p2, result);
}

inline size_t PitchDetector::findCepstrumBin(std::vector<double> &cepstrum)
{
    double bestX = -1;
    double bestValue = -std::numeric_limits<double>::max();
    bool peaked = false;

    bool firstPeak = true;
    for (int i = this->minimumCepstrumBin; i < this->maximumCepstrumBin; ++i)
    {
        double currentValue = cepstrum[i];
        if (firstPeak)
        {
            // do NOT pick up noise spikes on very broad first peak in the 
            // cepstrum.
            if (currentValue > 0.4) 
                continue; // ignore.
            firstPeak = false; // start processing.
        } else {
            if (currentValue > cepstrum[i - 1] && currentValue > cepstrum[i + 1])
            {
                if (currentValue > bestValue)
                {
                    peaked = true;
                    bestValue = currentValue;
                    bestX = i;
                }
            }
            if (peaked && currentValue < bestValue * 0.5)
            {
                // while in the same run of high values, anything better is better.
                // But we are well past the peak, so the next peak must be MUCH better than this one.
                // The alternative is to do cubic evaluation to avoid quantization noise around the each
                // candidate peak.
                bestValue *= 2;
                peaked = false;
            }
        }
    }
    if (bestX == -1)
    {
        return 0;
    }


#if 0
    // debug: dump fft/cepstrum buffer for external analysis.
    {
        std::filesystem::path fname =
            std::filesystem::path(getenv("HOME")) / "testOutput";
        std::filesystem::create_directories(fname);
        fname /= "cepstrum.csv";

        std::ofstream f(fname);

        for (size_t i = 1; i < cepstrumBuffer.size()/2-1; ++i)
        {
            auto v = std::abs(cepstrumBuffer[i]);
            f << sampleRate/(double)i/2 << "," << v << std::endl;
        }
    }
#endif


#if 0
    // debug: dump fft/cepstrum buffer for external analysis.
    {
        std::filesystem::path fname =
            std::filesystem::path(getenv("HOME")) / "testOutput";
        std::filesystem::create_directories(fname);
        fname /= "fft.csv";

        std::ofstream f(fname);

        for (size_t i = 0; i < fftBuffer.size()/2-1; ++i)
        {
            auto v = std::abs(fftBuffer[i]);
            f << i*(double)sampleRate/cepstrumFftSize << "," << v << std::endl;
        }
    }
#endif

    return bestX;
}

double PitchDetector::getGrandkeEstimate(double frequency)
{
    return getGrandkeEstimate(frequency,frequency);
}
double PitchDetector::getGrandkeEstimate(double minFrequency, double maxFrequency)
{
    size_t minBin = (size_t)std::floor(minFrequency*cepstrumFftSize/sampleRate)-1;
    size_t maxBin = (size_t)std::ceil(maxFrequency*cepstrumFftSize/sampleRate)+1;
    if (minBin < 0) return 0;

    size_t bin = size_t(-1);
    double bestVal = -1;
    for (size_t i = minBin; i <= maxBin; ++i)
    {
        double val = abs2(fftBuffer[i]);
        if (val > bestVal)
        {
            bin = i;
            bestVal = val;
        }
    }
    if (bin == size_t(-1)) return 0;

    double alpha = std::abs(fftBuffer[bin]) / std::abs(fftBuffer[bin + 1]);
    double delta = (2 * alpha - 1) / (alpha + 1);

    double t = bin + 1 - delta;
    return t * sampleRate / cepstrumFftSize;
}


double PitchDetector::ifPhase(size_t bin)
{
    std::complex<double> t = fftBuffer[bin] / lastFftBuffer[bin];

    double phase = atan2(t.imag(), t.real());
    return phase / Pi;
}

double PitchDetector::detectPitch(size_t sampleStride)
{
    this->lastFftBuffer = fftBuffer;

    double result = detectPitch();
    if (sampleStride != 0)
    {

        size_t bin = size_t(result * cepstrumFftSize / sampleRate);

        double phase0 = ifPhase(bin);
        double phase1 = ifPhase(bin + 1);

        double ifResult;
        if (phase0 < 0)
        {
            ifResult = (bin + 1 + phase1) * sampleRate / cepstrumFftSize;
        }
        else
        {
            ifResult = (bin + phase0) * sampleRate / cepstrumFftSize;
        }
        return ifResult;
    }
    return result;
}
double PitchDetector::detectPitch()
{

    fftPlan.Forward(scratchBuffer, fftBuffer);

    for (int i = 0; i < cepstrumFftSize; ++i)
    {
        complex t = fftBuffer[i];

        scratchBuffer[i] = std::abs(t);
    }

    fftPlan.Forward(scratchBuffer, cepstrumBuffer);

    for (size_t i = 0; i < cepstrum.size(); ++i)
    {
        complex t = cepstrumBuffer[i];
        cepstrum[i] = abs(t);
    }

    // find the fundamental frequency, approximately, inferring fundamentals if neccessary.
    int cepstrumBin = findCepstrumBin(cepstrum);

    if (cepstrumBin <= 0) return 0;

    // Cepstrum is succeptible to noise. 
    // Determine the range of frequencies in the bin,
    // and then calculate the frequency using the Grandke interpolation of the bin 
    // with the maximum peak in the given range.
    double maxFrequency = sampleRate/double(cepstrumBin-2)/2;
    double minFrequency = sampleRate/double(cepstrumBin+2)/2;

    // sharpen the estimate using Grandke interpolation.
    return getGrandkeEstimate(minFrequency,maxFrequency);
}

double PitchDetector::refineWithCrossCorrelation(std::vector<double> &crossCorrelation, double cepstrumFrequency)
{
    size_t crossCorrelationBin = (size_t)(sampleRate / cepstrumFrequency);
    double p0, p1, p2;
    while (true)
    {
        p0 = log(crossCorrelation[crossCorrelationBin - 1]);
        p1 = log(crossCorrelation[crossCorrelationBin]);
        p2 = log(crossCorrelation[crossCorrelationBin + 1]);
        if (p0 > p1)
        {
            --crossCorrelationBin;
            if (p2 > p1)
            {
                return false;
            }
        }
        else if (p2 > p1)
        {
            ++crossCorrelationBin;
        }
        else
        {
            break;
        }
    }
    QuadResult quadResult;
    if (findQuadraticMaximum(crossCorrelationBin, p0, p1, p2, quadResult))
    {
        double correlationResult = this->sampleRate / quadResult.x;


        return correlationResult;
    }
    return 0;
}

double PitchDetector::frequencyToBin(double frequency)
{
    return frequency * sampleRate / getFftSize();
}
double PitchDetector::binToFrequency(double bin)
{
    return bin * getFftSize() / sampleRate;
}
