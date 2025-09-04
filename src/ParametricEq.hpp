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
#pragma once

#include "Filters/AudioFilter2.h"
#include "Filters/ShelvingFilter.h"
#include "Filters/LowPassFilter.h"
#include "Filters/HighPassFilter.h"
#include "Filters/PeakingFilter2.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <complex>

class BellFilter {
private:
    // Biquad coefficients
    double b0, b1, b2, a1, a2;
    
    // State variables for filtering
    double x1, x2, y1, y2;
    
    // Filter parameters
    double sampleRate;
    double centerFreq;
    double gainDB;
    double bandwidth;
    
public:
    BellFilter(double fs = 44100.0) 
        : sampleRate(fs), centerFreq(1000.0), gainDB(0.0), bandwidth(1.0),
          b0(1.0), b1(0.0), b2(0.0), a1(0.0), a2(0.0),
          x1(0.0), x2(0.0), y1(0.0), y2(0.0) {}

    void  SetSampleRate(double fs) 
    {
        this->sampleRate = fs;
    }
    void SetParameters(double freq, double gain_dB, double Q) {
        centerFreq = freq;
        gainDB = gain_dB;
        bandwidth = freq / Q;
        calculateCoefficients();
    }
    
    void calculateCoefficients() {
         double omega = 2.0 * M_PI * centerFreq / sampleRate;
        double sin_omega = sin(omega);
        double cos_omega = cos(omega);
        double A = pow(10.0, gainDB / 40.0); // sqrt of linear gain
        double Q = centerFreq / bandwidth; // Convert bandwidth to Q
        double alpha = sin_omega / (2.0 * Q);
        
        // Bell filter coefficients (RBJ cookbook)
        b0 = 1.0 + alpha * A;
        b1 = -2.0 * cos_omega;
        b2 = 1.0 - alpha * A;
        double a0 = 1.0 + alpha / A;
        a1 = -2.0 * cos_omega;
        a2 = 1.0 - alpha / A;
        
        // Normalize by a0
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }
    
    double Tick(double input) {
        // Direct Form II implementation
        double output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        
        // Update state variables
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        
        return output;
    }
    
    
    void Reset() {
        x1 = x2 = y1 = y2 = 0.0;
    }
    
    // Get frequency response at a given frequency
    double GetFrequencyResponse(double freq) {
        double omega = 2.0 * M_PI * freq / sampleRate;
        std::complex<double> z = std::exp(std::complex<double>(0, -omega));
        std::complex<double> z2 = z * z;
        
        std::complex<double> numerator = b0 + b1 * z + b2 * z2;
        std::complex<double> denominator = 1.0 + a1 * z + a2 * z2;
        std::complex<double> H = numerator / denominator;
        
        return abs(H);
    }
};




namespace toob {
    class ParametricEq {
    public:
        
        void SetSampleRate(double sampleRate);

        HighPassFilter lowCut;
        LowPassFilter highCut;

        ShelvingFilter lowShelf;    
        ShelvingFilter highShelf;    
        // PeakingFilter2 lmf;
        // PeakingFilter2 hmf;
        BellFilter lmf;
        BellFilter hmf;

        float Tick(float value)
        {
            return lowCut.Tick(
                highCut.Tick(
                    lowShelf.Tick(
                        highShelf.Tick(
                            lmf.Tick(
                                hmf.Tick(value)
                            )
                        )
                    )
                )
            );
        }
        double GetFrequencyResponse(float f);
    private: 
        double sampleRate;

    };

}