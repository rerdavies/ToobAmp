
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

namespace LsNumerics
{

    // FFT direction specifier
    enum class fft_dir
    {
        forward = +1,
        backward = -1
    };

    template <typename T>
    class Fft
    {
    private:
        std::vector<std::complex<T>> forwardTwiddle;
        std::vector<std::complex<T>> backwardTwiddle;
        std::vector<uint32_t> bitReverse;
        std::vector<std::complex<T>> windowedData;
        T norm;
        size_t log2N;
        size_t N = -1;

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
        static uint32_t bitr(uint32_t x, size_t nb)
        {
            assert(nb > 0 && nb <= 32);
            x = (x << 16) | (x >> 16);
            x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
            x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
            x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
            x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);

            return ((x >> (32 - nb)) & (0xFFFFFFFF >> (32 - nb)));
        }
        void calculateTwiddleFactors(fft_dir dir, std::vector<std::complex<T>> &twiddles)
        {
            twiddles.resize(0);

            for (size_t i = 1; i <= log2N; ++i)
            {
                size_t m = 1 << i;  // butterfly mask
                size_t m2 = m >> 1; // butterfly width
                // fft butterflies

                size_t wI = 0;
                for (size_t j = 0; j < m2; ++j)
                {
                    twiddles.push_back(std::exp(std::complex<T>(0, -wI * Pi / m2 * T(dir))));
                    ++wI;
                }
            }
        }

    public:
        Fft() {}
        Fft(size_t size)
        {
            SetSize(size);
        }
        size_t GetSize() const { return N; }
        void SetSize(size_t size)
        {

            if (this->N == size)
            {
                return;
            }
            assert((size & (size - 1)) == 0); // must be power of 2!

            this->N = size;
            bitReverse.resize(N);
            windowedData.resize(N);

            log2N = log2(N);

            for (size_t j = 0; j < N; ++j)
            {
                bitReverse[j] = bitr(j, log2N);
            }
            norm = T(1 / std::sqrt(double(N)));
            calculateTwiddleFactors(fft_dir::forward, forwardTwiddle);
            calculateTwiddleFactors(fft_dir::backward, backwardTwiddle);
        }
        void compute(const std::vector<std::complex<T>> &input, std::vector<std::complex<T>> &output, fft_dir dir)
        {
            assert(N != -1);
            assert(input.size() >= (size_t)N);
            assert(output.size() >= (size_t)N);

            size_t cnt = N;
            // pre-process the input data using a borrowed buffer
            // in case input and output are aliased.
            if (&input == &output)
            {
                for (size_t j = 0; j < cnt; ++j)
                {
                    windowedData[j] = norm * input[bitReverse[j]];
                }
                for (size_t j = 0; j < cnt; ++j)
                {
                    output[j] = windowedData[j];
                }
            }
            else
            {
                for (size_t j = 0; j < cnt; ++j)
                {
                    output[j] = norm*input[bitReverse[j]];
                }
            }
            computeInner(output,dir);
        }
        void compute(const std::vector<float> &input, std::vector<std::complex<T>> &output, fft_dir dir)
        {
            assert(N != -1);
            assert(input.size() >= (size_t)N);
            assert(output.size() >= (size_t)N);

            size_t cnt = N;
            // pre-process the input data
            for (size_t j = 0; j < cnt; ++j)
                output[j] = norm * input[bitReverse[j]];
            computeInner(output,dir);
        }

        void forward(const std::vector<std::complex<T>> &input, std::vector<std::complex<T>> &output)
        {
            compute(input, output, fft_dir::forward);
        }
        void backward(const std::vector<std::complex<T>> &input, std::vector<std::complex<T>> &output)
        {
            compute(input, output, fft_dir::backward);
        }

        template <typename U>
        void forward(const std::vector<U> &input, std::vector<std::complex<T>> &output)
        {
            for (size_t i = 0; i < N; ++i)
            {
                windowedData[i] = (T)(input[i]);
            }
            compute(windowedData, output, fft_dir::forward);
        }
        template <typename U>
        void backward(const std::vector<U> &input, std::vector<std::complex<T>> &output)
        {
            for (size_t i = 0; i < N; ++i)
            {
                windowedData[i] = input[i];
            }
            compute(windowedData, input, fft_dir::backward);
        }
        template <typename U>
        void backward(const std::vector<std::complex<T>> &input, std::vector<std::complex<U>> &output)
        {
            compute(input, windowedData, fft_dir::backward);
            for (size_t i = 0; i < N; ++i)
            {
                output[i] = (U)(windowedData[i].real());
            }
        }

        template <typename U>
        void forwardWindowed(const std::vector<U> &window, const std::vector<U> &input, std::vector<std::complex<T>> &output)
        {
            for (size_t i = 0; i < N; ++i)
            {
                windowedData[i] = (T)(window[i] * input[i]);
            }
            return compute(windowedData, output, fft_dir::forward);
        }

    private:
        void computeInner(std::vector<std::complex<T>> &output, fft_dir dir)
        {
            // fft passes
            for (size_t i = 1; i <= log2N; ++i)
            {
                size_t m = 1 << i;  // butterfly mask
                size_t m2 = m >> 1; // butterfly width

                std::complex<double> wj(1, 0);
                std::complex<double> wInc = std::exp(std::complex<double>(0, Pi / m2 * double(dir)));

                // fft butterflies
                for (size_t j = 0; j < m2; ++j)
                {
                    std::complex<T> w = std::complex<T>((T)wj.real(), (T)wj.imag());
                    for (size_t k = j; k < N; k += m)
                    {
                        std::complex<T> t = w * output[k + m2];
                        std::complex<T> u = output[k];
                        output[k] = u + t;
                        output[k + m2] = u - t;
                    }
                    wj *= wInc;
                }
            }
        }
    };

} // namespace

#endif // DJ_INCLUDE_FFT_H
