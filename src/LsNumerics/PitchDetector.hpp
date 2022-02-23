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

#pragma once

#include <cstdint>
#include "Fft.hpp"
#include <vector>
#include "LsMath.hpp"

namespace LsNumerics {

class PitchDetector {
    Fft<double> fft;
    Fft<double> paddedFft;
private:

    int fftSize;
    int minimumCepstrumBin;
    int maximumCepstrumBin;

    float referencePitch = 440.0f;


    bool debug;

    using complex = std::complex<double>;

    int sampleRate;
    std::vector<complex> stagingBuffer;



    std::vector<double> window;
    std::vector<complex> conversionBuffer;
    std::vector<complex>  scratch;
    std::vector<complex>  scratch2;
    std::vector<double>  autoCorrelation;
    std::vector<double>  cepstrum;
    std::vector<double>  speciallyNormalizedAutoCorrelation;



private:
    void allocateBuffers();
    double findCepstrumFrequency(std::vector<double>& cepstrum);

public:
    PitchDetector();
    // Choose optimimum fft size.
    PitchDetector(int sampleRate);

    // Force the fft size.
    PitchDetector(int sampleRate, int fftSize);

    void Initialize(int sampleRate);
    void Initialize(int sampleRate, int fftSize);

    size_t getFftSize() const { return this->fftSize; }

    double detectPitch(short* signal) {
        for (int i = 0; i < fftSize; ++i) {
            conversionBuffer[i] = signal[i] * (1.0 / 32768);
        }
        return detectPitch(conversionBuffer);
    }

    template <typename ITERATOR>
    double detectPitch(const ITERATOR &begin, const ITERATOR&end)
    {
        ITERATOR iter = begin;
        for (int i = 0; i < fftSize; ++i) {
            conversionBuffer[i] = *(iter);
            ++iter;
        }
        return detectPitch(conversionBuffer);

    }
    double detectPitch(float* signal) {
        for (int i = 0; i < fftSize; ++i) {
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

    struct CubicResult {
        double x;
        double y;
    };

private:

    double detectPeaks(std::vector<double>& x, double suggestedFrequency);




    bool findCubicMaximum(int binNumber, std::vector<double>& x, CubicResult &result);
        

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