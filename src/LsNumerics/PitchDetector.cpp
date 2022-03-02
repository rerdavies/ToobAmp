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
#include <fstream>
#include <limits>
#include "Window.hpp"
#include <exception>

using namespace LsNumerics;

static constexpr double GUITAR_LOW_E_FREQUENCY = 82.41;        // hz.
static constexpr double MAXIMUM_DETECTABLE_FREQUENCY = 923.33; // gutar, high E, 19th fret.
static constexpr double MINIMUM_DETECTABLE_FREQUENCY = 75;     // guitar low E - 1/2 semi-tone.
static constexpr double OCTAVE_THRESHOLD = 0.5;

static constexpr int WINDOW_PERIODS_REQUIRED = 4;

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
    this->cepstrumFft.SetSize(fftSize);

#if LS_ENABLE_AUTO_CORRELATION_CODE
    this->crossCorrelationSize = this->cepstrumFftSize;
    this->crossCorrelationSamples = crossCorrelationSize / 2;
    this->crossCorrelationFft.SetSize(crossCorrelationSize);
#endif

    this->window = Window::ExactBlackman<double>(fftSize);
    allocateBuffers();
    // f = this->sampleRate/(cepstrumIndex)
    this->minimumCepstrumBin = (int)(sampleRate / MAXIMUM_DETECTABLE_FREQUENCY);
    this->maximumCepstrumBin = (int)(sampleRate / MINIMUM_DETECTABLE_FREQUENCY);

    frequencyAdustmentFactor = calculateFrequencyAdjustmentFactor();    
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
    this->scratch.resize(scratchSize);
    this->scratch2.resize(scratchSize);
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

inline double PitchDetector::findCepstrumFrequency(std::vector<double> &cepstrum)
{
    double dx = 1.0 / cepstrumFftSize;
    size_t n = cepstrum.size();
    double bestX = -1;
    double bestValue = -std::numeric_limits<double>::max();
    bool peaked = false;

    for (int i = 1; i < cepstrum.size(); ++i)
    {
        double currentValue = cepstrum[i];
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
            bestValue *= 1.2;
            peaked = false;
        }
    }
    if (bestX == -1)
    {
        return 0;
    }

    QuadResult cepstrumResult;
    if (findQuadraticMaximumNoLog(bestX, cepstrum, cepstrumResult))
    {
        double frequency = this->sampleRate / (cepstrumResult.x);

        // see calculateFrequencyAdjustmentFactor() for details.
        return frequency - frequencyAdustmentFactor / frequency;
    }
    return 0;
}

double PitchDetector::calculateFrequencyAdjustmentFactor()
{
    // Completely empirical fudge to improve accuracy.
    // unable to account for this correction theoretically,
    // but the results are spookily exact.

    // As far as I can see, there's no easy formulaic closed-form
    // for the observed constants.

    // *PERHAPS*, it's caused by the choice of window function.
    // Auto-correlation gets a bias because the data is ever so
    // slightly chirped ( sin(t)/(1+t)), which can be compensated
    // for by normalizing amplitudes based on bin number.
    //
    // Cepstrum probably suffers from a similar defect. For what it's
    // worth, the deep theory of the cepstrum method is pretty 
    // spotty, and the kind of precision we're pushing for 
    // (~0.1 cents in useful instrument rage) is well beyond the
    // precision acheived in the literature.

    // Values were determined by curve-fitting measured test data.
    // Note that there's an additional phase-related problem in 
    // high frequencies which is not accounted for, so values 
    // were calculated over ~80hz-~250hz range.

    // These values produce less than 1 cent error in the range
    // 80hz-800hz, with typical errors of < 0.1 cent in normal instrument
    // range. This coverers guitar up until high-e 12-th fret, after which
    // you will see a phase-related oscillation that increases as you go 
    // higher.
    static const double K_2048_22050 = 14.2663342608696;
    static const double K_2048_24000 = 16.7851051304348;

    static const double K_4096_22050 = 3.5040846;
    static const double K_4096_24000 = 4.15781873043478;

    static const double K_8192_22050 = 1.00869074956522;
    static const double K_8192_24000 = 1.15865716434783;

    if (sampleRate == 24000)
    {
        switch (this->getFftSize())
        {
            case 2048: return K_2048_24000;
            case 4096: return K_4096_24000;
            case 8192: return K_8192_24000;
            default: 
                throw std::invalid_argument("Unsupported FFT rate.");
        }
    }
    if (sampleRate == 22050)
    {
        switch (this->getFftSize())
        {
            case 2048: return K_2048_22050;
            case 4096: return K_4096_22050;
            case 8192: return K_8192_22050;
            default: 
                throw std::invalid_argument("Unsupported FFT rate.");
        }
    }
    // you must *downsample* audio date to either 24000hz or 22050hz sample rate.
    // (counterintuitively, this increases stability, accuracy and efficiency)
    throw std::invalid_argument("Unsupported sample rate.");


}

static inline double abs2(const std::complex<double> &c)
{
    return c.real() * c.real() + c.imag() * c.imag();
}

double PitchDetector::detectPitch(std::vector<complex> &signal)
{
    // autoCorrelation = cross-correlate(signal^2,window)

    // Cepstrum.
    for (int i = 0; i < cepstrumFftSize; ++i)
    {
        scratch[i] = window[i] * signal[i] * signal[i]; // non-linearity reduces cepstrum octave errors ;
    }
    cepstrumFft.forward(scratch, scratch2);

    for (int i = 0; i < cepstrumFftSize; ++i)
    {
        complex t = scratch2[i];

        scratch2[i] = std::abs(t);
    }

    cepstrumFft.forward(scratch2, scratch);

    for (size_t i = 0; i < cepstrum.size(); ++i)
    {
        complex t = scratch[i];
        cepstrum[i] = abs(t);
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
        for (int i = 0; i < scratch.size(); ++i)
        {
            double v = cepstrum[i];
            double v2 = (scratch[i]*std::conj(scratch[i])).real();
            f << v2 << "\t" << v <<  std::endl;
        }
        f << '\n';

    }
#endif
#if LS_ENABLE_AUTO_CORRELATION_CODE

    // auto-cross-correlation
    for (int i = 0; i < crossCorrelationSamples; ++i)
    {
        scratch[i] = signal[i];
    }
    for (int i = crossCorrelationSamples; i < crossCorrelationSize; ++i)
    {
        scratch[i] = 0;
    }
    crossCorrelationFft.forward(scratch, scratch2);
    for (int i = 0; i < crossCorrelationSize; ++i)
    {
        scratch2[i] = scratch2[i] * (scratch2[i]);
    }
    crossCorrelationFft.backward(scratch2, scratch);

    for (int i = 1; i < crossCorrelationSamples; ++i)
    {
        double k = 1 - (2.0 * i) / crossCorrelationSize;
        speciallyNormalizedAutoCorrelation[i] =
            abs(scratch[i]) / k;
    }
#if 0
    {
        std::ofstream f;
        f.open("/home/rerdavies/temp/data.tsv");
        assert(!f.fail());
        for (int i = 0; i < speciallyNormalizedAutoCorrelation.size(); ++i)
        {
            double v = (speciallyNormalizedAutoCorrelation[i]);
            f << v << '\n';
        }
        f << '\n';

    }
#endif
#endif

    double cepstrumFrequency = findCepstrumFrequency(cepstrum);

    // delete me:
    // double result = refineWithCrossCorrelation(speciallyNormalizedAutoCorrelation,cepstrumFrequency);
    // return result;
    return cepstrumFrequency;
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

        double expectedBin = this->sampleRate / std::round(correlationResult);

        return correlationResult;
    }
    return 0;
}
