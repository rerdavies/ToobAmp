
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

#pragma once

#ifndef LSNUMERICS_INCLUDE_FFT_HPP
#define LSNUMERICS_INCLUDE_FFT_HPP

#include <complex> // std::complex
#include <vector>  // std::vector

#include <cmath>
#include <cstdint>
#include <cassert>
#include "LsMath.hpp"
#include "Window.hpp"

#ifndef RESTRICT
#define RESTRICT __restrict  // equivalent of C99 restrict keyword. Valid for MSVC,GCC and CLANG.
#endif
namespace LsNumerics
{


    class Fft
    {
    public:
            // FFT direction specifier
        enum class fft_dir
        {
            forward = +1,
            backward = -1
        };

    
    private:
        static constexpr size_t UNINITIALIZED_VALUE = (size_t)-1;
        std::vector<std::complex<double>> forwardTwiddle;
        std::vector<std::complex<double>> backwardTwiddle;
        std::vector<uint32_t> bitReverse;
        std::vector<std::complex<double>> windowedData;
        double norm;
        size_t log2N;
        size_t fftSize = UNINITIALIZED_VALUE;

        static size_t log2(size_t x)
        {
            size_t result = 0;

            while (x > 1)
            {
                x >>= 1;
                ++result;
            }

            return result;
        }

        /*
         *  Bit reverse an integer given a word of nb bits
         *  NOTE: Only works for 32-bit words max
         *  examples:
         *  10b      -> 01b
         *  101b     -> 101b
         *  1011b    -> 1101b
         *  0111001b -> 1001110b
         */
        void CalculateTwiddleFactors(fft_dir dir, std::vector<std::complex<double>> &twiddles);

    public:
        Fft() {}
        Fft(size_t size)
        {
            SetSize(size);
        }
        size_t GetSize() const { return fftSize; }
        void SetSize(size_t size);
        
        void Compute(const std::vector<std::complex<double>> &input, std::vector<std::complex<double>> &output, fft_dir dir);
        void Compute(const std::vector<float> &input, std::vector<std::complex<double>> &output, fft_dir dir);

        void Forward(const std::vector<std::complex<double>> &input, std::vector<std::complex<double>> &output)
        {
            Compute(input, output, fft_dir::forward);
        }
        void Backward(const std::vector<std::complex<double>> &input, std::vector<std::complex<double>> &output)
        {
            Compute(input, output, fft_dir::backward);
        }

        template <typename U>
        void Forward(const std::vector<U> &input, std::vector<std::complex<double>> &output)
        {
            for (size_t i = 0; i < fftSize; ++i)
            {
                windowedData[i] = (double)(input[i]);
            }
            Compute(windowedData, output, fft_dir::forward);
        }
        template <typename U>
        void Backward(const std::vector<U> &input, std::vector<std::complex<double>> &output)
        {
            for (size_t i = 0; i < fftSize; ++i)
            {
                windowedData[i] = input[i];
            }
            Compute(windowedData, input, fft_dir::backward);
        }
        template <typename U>
        void Backward(const std::vector<std::complex<double>> &input, std::vector<std::complex<U>> &output)
        {
            Compute(input, windowedData, fft_dir::backward);
            for (size_t i = 0; i < fftSize; ++i)
            {
                output[i] = (U)(windowedData[i].real());
            }
        }

        template <typename U>
        void ForwardWindowed(const std::vector<U> &window, const std::vector<U> &input, std::vector<std::complex<double>> &output)
        {
            for (size_t i = 0; i < fftSize; ++i)
            {
                windowedData[i] = (double)(window[i] * input[i]);
            }
            return Compute(windowedData, output, fft_dir::forward);
        }

    private:
        void ComputeInner(std::vector<std::complex<double>> &output, fft_dir dir);
    };

} // namespace

#endif // DJ_INCLUDE_FFT_H
