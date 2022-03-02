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

/********************************************************
 * class PitchDetector - pitch detector optimized for use as in a guitar tuner.
 * 
 * PitchDetector currently only works with a sample rate of 22050, or 24000hz.
 * (There are manually fittted adjustments that improve pitch accuracy which 
 * currently can't be accounted for in closed form). 
 * 
 * Downssampling is neccessary both for efficiency and stability. Running
 * pitch detection at higher sample rates does not increase accuracy, 
 * and introduces instability problems that are not present at lower sample
 * rates.
 * 
 * Ideally you should decimate your audio signal after running it through a 
 * suitable lowpass anti-aliasing filter. Setting the anti-aliasing filter
 * fC to 1200hz would not be wrong, as the pitch detector doesn't work 
 * well above that frequency.
 * 
 * Measured accuracy: stable tuning with ~0.1 cents accuracy in normal guitar 
 * tuning range. Less than 1 cent below high-e 12th fret, with osciallations 
 * due to phase errors increasing above the 12th fret. You could filter to
 * reduce those oscillations if you REALLY need pitch detection in that range.
 * 
 * NOT suitable for use on a realtime thread!

*/
#pragma once

#include <cstdint>
#include "Fft.hpp"
#include <vector>
#include "LsMath.hpp"

// auto-correlation was tested and found less accurate. 
// keep the code for now.
#define LS_ENABLE_AUTO_CORRELATION_CODE 0


namespace LsNumerics {

class PitchDetector {
    Fft<double> cepstrumFft;
private:

    int cepstrumFftSize;

    int minimumCepstrumBin;
    int maximumCepstrumBin;
    double frequencyAdustmentFactor = 0;

    double calculateFrequencyAdjustmentFactor();

    float referencePitch = 440.0f;


    bool debug;

    using complex = std::complex<double>;

    int sampleRate;
    std::vector<complex> stagingBuffer;



    std::vector<double> window;
    std::vector<complex> conversionBuffer;
    std::vector<complex>  scratch;
    std::vector<complex>  scratch2;
    std::vector<double>  cepstrum;

#if LS_ENABLE_AUTO_CORRELATION_CODE    
    Fft<double> crossCorrelationFft;
    int crossCorrelationSize;
    int crossCorrelationSamples;
    std::vector<double>  autoCorrelation;
    std::vector<double>  speciallyNormalizedAutoCorrelation;
#endif



private:
    void allocateBuffers();
    double findCepstrumFrequency(std::vector<double>& cepstrum);
    double refineWithCrossCorrelation(std::vector<double>&crossCorrelation,double cepstrumFrequency);

public:
    PitchDetector();
    // Choose optimimum fft size.
    PitchDetector(int sampleRate);

    // Force the fft size.
    PitchDetector(int sampleRate, int fftSize);

    void Initialize(int sampleRate);
    void Initialize(int sampleRate, int fftSize);

    size_t getFftSize() const { return this->cepstrumFftSize; }

    double detectPitch(short* signal) {
        for (int i = 0; i < cepstrumFftSize; ++i) {
            conversionBuffer[i] = signal[i] * (1.0 / 32768);
        }
        return detectPitch(conversionBuffer);
    }

    template <typename ITERATOR>
    double detectPitch(const ITERATOR &begin, const ITERATOR&end)
    {
        ITERATOR iter = begin;
        for (int i = 0; i < cepstrumFftSize; ++i) {
            conversionBuffer[i] = *(iter);
            ++iter;
        }
        return detectPitch(conversionBuffer);

    }
    double detectPitch(float* signal) {
        for (int i = 0; i < cepstrumFftSize; ++i) {
            conversionBuffer[i] = signal[i];
        }
        return detectPitch(conversionBuffer);
    }
private:
    double detectPitch(std::vector<complex> &signal);
public:

    float getReferencePitch() {
        return referencePitch;
    }

    void setReferencePitch(float referencePitch) {
        this->referencePitch = referencePitch;
    }

    struct QuadResult {
        double x;
        double y;
    };

private:

    double detectPeaks(std::vector<double>& x, double suggestedFrequency);




    bool findQuadraticMaximum(int binNumber, std::vector<double>& x, QuadResult &result);
    bool findQuadraticMaximumNoLog(int binNumber, std::vector<double>& x, QuadResult &result);
    bool findQuadraticMaximum(int binNumber, double p0, double p1, double p2, QuadResult &result);
        

    double binToFrequency(double bin) {
        return sampleRate / (bin);
    }

    int frequencyToBin(double frequency) {
        // f = sampleRate/(bin*2)
        double bin = sampleRate / (frequency);
        return (int) bin;
    }
};
} // namespace.