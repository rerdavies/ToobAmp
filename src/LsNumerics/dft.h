
#ifndef LSNUMERICS_INCLUDE_FFT_H
#define LSNUMERICS_INCLUDE_FFT_H

#include <complex> // std::complex
#include <vector>  // std::vector

#include <cmath>
#include <cstdint>
#include <cassert>
#include "LsMath.hpp"

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


namespace LsNumerics
{



    // FFT direction specifier
    enum class fft_dir
    {
        forward = +1,
        backward = -1
    };



    template <typename T>
    class Dft
    {
    private:

        std::vector<int> bitReverse;
        std::vector<std::complex<T> > result;
        std::vector<T> window;
        std::vector<T> windowedData;
        T norm;
        int log2N;
        int N = -1;

        static int log2(int x)
        {
            int result = 0;

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
        static int bitr(uint32_t x, int nb)
        {
            assert(nb > 0 && nb <= 32);
            x = (x << 16) | (x >> 16);
            x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
            x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
            x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
            x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);

            return ((x >> (32 - nb)) & (0xFFFFFFFF >> (32 - nb)));
        }


    public:
        Dft(int size)
        {
            SetSize(size);
        }
        void SetSize(int size)
        {
            if (this->N == size) 
            {
                return;
            }
            this->N = size;
            bitReverse.resize(N);
            result.resize(N);
            window.resize(N);
            windowedData.resize(N);
            
            assert((N & (N-1)) == 0); // must be power of 2!

            log2N = log2(N);

            for (int j = 0; j < N; ++j)
            {
                bitReverse[j] = bitr(j, log2N);
            }
            norm = T(1) / std::sqrt(T(N));

            // Exact Blackman window. See https://en.wikipedia.org/wiki/Window_function#Blackman_window
            double a0 =  7938.0/18608.0;
            double a1 = 9240.0/18608.0;
            double a2 = 1430.0/18608.0;
            for (size_t i = 0; i < N; ++i)
            {
                window[i] = a0 - a1*std::cos(2*Pi/N*i)+a2*std::cos(4*Pi/N*i);
            }
        }

        const std::vector<std::complex<T> >& compute(const std::vector<std::complex<T> > &input, const fft_dir &dir)
        {
            assert(input.size() == N);

            std::vector<std::complex<T> >& result = this->result;

            // pre-process the input data
            for (int j = 0; j < N; ++j)
                result[j] = norm * input[bitReverse[j]];

            // fft passes
            for (int i = 1; i <= log2N; ++i)
            {
                int m = 1 << i;    
                int m2 = m >> 1;   
                std::complex<T> wj(1,0);
                std::complex<T> wInc = std::exp(std::complex<T>(0,-Pi/m2*T(dir)));
                // fft butterflies
                for (int j = 0; j < m2; ++j)
                {
                    for (int k = j; k < N; k += m)
                    {
                        std::complex<T> t = wj*result[k+m2];
                        std::complex<T> u = result[k];
                        result[k] = u+t;
                        result[k+m2] = u-t;
                    }
                    wj *= wInc;
                }
            }

            return result;
        }
        const std::vector<std::complex<T> >& compute(const std::vector<T> &input, const fft_dir &dir)
        {
            assert(input.size() == N);
            int cnt = N;

            std::vector<std::complex<T> >& xo = this->result;

            // pre-process the input data
            for (int j = 0; j < cnt; ++j)
                xo[j] = norm * input[bitReverse[j]];

            // fft passes
            for (int i = 1; i <= log2N; ++i)
            {
                int m = 1 << i;             // butterfly mask
                int m2 = m >> 1;             // butterfly width
                std::complex<T> wj(1,0);
                std::complex<T> wInc = std::exp(std::complex<T>(0,-Pi/m2*T(dir)));
                // fft butterflies
                for (int j = 0; j < m2; ++j)
                {
                    for (int k = j; k < N; k += m)
                    {
                        std::complex<T> t = wj*xo[k+m2];
                        std::complex<T> u = xo[k];
                        xo[k] = u+t;
                        xo[k+m2] = u-t;
                    }
                    wj *= wInc;
                }
            }

            return xo;
        }
        const std::vector<std::complex<T> >& forward(const std::vector<std::complex<T> > &input)
        {
            return compute(input,fft_dir::forward);
        }
        const std::vector<std::complex<T> >& backward(const std::vector<std::complex<T> > &input)
        {
            return compute(input,fft_dir::backward);
        }
        const std::vector<std::complex<T> >& forward(const std::vector<T> &input)
        {
            return compute(input,fft_dir::forward);
        }
        const std::vector<std::complex<T> >& forwardWindowed(const std::vector<float> &input)
        {
            for (int i = 0; i < N; ++i)
            {
                windowedData[i] = window[i]*input[i];
            }
            return compute(windowedData,fft_dir::forward);
        }
        const std::vector<std::complex<T> >& forwardWindowed(const std::vector<double> &input)
        {
            for (int i = 0; i < N; ++i)
            {
                windowedData[i] = window[i]*input[i];
            }
            return compute(windowedData,fft_dir::forward);
        }
    };


} // namespace 

#endif // DJ_INCLUDE_FFT_H


