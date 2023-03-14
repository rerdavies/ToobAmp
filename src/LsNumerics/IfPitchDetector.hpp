/*
 * MIT License
 * 
 * Copyright (c) 2022 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include "Fft.hpp"

namespace LsNumerics {
    using namespace std;

    class IfPitchDetector {
    public:
        using complex_t = complex<double>;
        using buffer_t = vector<complex_t>;

        IfPitchDetector(double sampleRate, size_t fftSize)
        :fftPlan(fftSize),
         fftSize(fftSize)
        {
            buffer0.resize(fftSize);
            buffer1.resize(fftSize);
            phase.resize(fftSize/2);

            window = Window::Hann<double>(fftSize);
            
        }

        void prime(std::vector<float> p, size_t index);
        double detectPitch(std::vector<float> p, size_t index,size_t sampleStride);
        size_t getFftSize() const { return fftSize;}



    private:
        Fft fftPlan;
        size_t fftSize;

        std::vector<double> window;
        buffer_t windowBuffer;

        std::vector<double> phase;

        buffer_t buffer0;
        buffer_t buffer1;

        buffer_t*fftBuffer = &buffer0;
        buffer_t*lastBuffer = &buffer1;

        void swapBuffers() {
            buffer_t *t = fftBuffer;
            fftBuffer = lastBuffer;
            lastBuffer = t;
        }

    };
}