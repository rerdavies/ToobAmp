
#ifndef LSNUMERICS_INCLUDE_FFT_H
#define LSNUMERICS_INCLUDE_FFT_H

#include <complex> // std::complex
#include <vector>  // std::vector

#include <cmath>
#include <cstdint>
#include <cassert>
#include "LsMath.hpp"
#include "Window.hpp"

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
    class Fft
    {
    private:

        std::vector<int> bitReverse;
        std::vector<std::complex<T>> windowedData;
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
        Fft() { }
        Fft(int size)
        {
            SetSize(size);
        }
        int GetSize() const { return N; }
        void SetSize(int size)
        {

            if (this->N == size) 
            {
                return;
            }
            assert((size & (size-1)) == 0); // must be power of 2!


            this->N = size;
            bitReverse.resize(N);
            windowedData.resize(N);
            

            log2N = log2(N);

            for (int j = 0; j < N; ++j)
            {
                bitReverse[j] = bitr(j, log2N);
            }
            norm = T(1) / std::sqrt(T(N));

        }


        void compute(const std::vector<std::complex<T> > &input,std::vector<std::complex<T> > &output, fft_dir dir)
        {
            assert(input.size() >= N);
            assert(output.size() >= N);
            int cnt = N;

            // pre-process the input data
            for (int j = 0; j < cnt; ++j)
                output[j] = norm * input[bitReverse[j]];

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
                        std::complex<T> t = wj*output[k+m2];
                        std::complex<T> u = output[k];
                        output[k] = u+t;
                        output[k+m2] = u-t;
                    }
                    wj *= wInc;
                }
            }

        }


        void forward(const std::vector<std::complex<T> > &input, std::vector<std::complex<T> > &output)
        {
            compute(input,output,fft_dir::forward);
        }
        void backward(const std::vector<std::complex<T> > &input, std::vector<std::complex<T> > &output)
        {
            compute(input,output,fft_dir::backward);
        }

        template <typename U> 
        void forward(const std::vector<U> &input, std::vector<std::complex<T> > &output)
        {
            for (int i = 0; i < N; ++i)
            {
                windowedData[i] = (T)(input[i]);
            }
            compute(windowedData,input,fft_dir::forward);

        }
        template <typename U> 
        void backward(const std::vector<U> &input, std::vector<std::complex<T> > &output)
        {
            for (int i = 0; i < N; ++i)
            {
                windowedData[i] = input[i];
            }
            compute(windowedData,input,fft_dir::backward);

        }

        template <typename U> 
        void forwardWindowed(const std::vector<U> &window,const std::vector<U> &input,std::vector<std::complex<T> > &output)
        {
            for (int i = 0; i < N; ++i)
            {
                windowedData[i] = (T)(window[i]*input[i]);
            }
            return compute(windowedData,output,fft_dir::forward);
        }
    };


} // namespace 

#endif // DJ_INCLUDE_FFT_H


