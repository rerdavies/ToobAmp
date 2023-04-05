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
#include <cstdint>
#include <cstddef>
#include <vector>
#include "LsMath.hpp"

namespace LsNumerics
{

    class Window
    {
    public:
        template <typename T>
        static std::vector<T> ExactBlackman(size_t N)
        {
            std::vector<T> window;
            window.resize(N);
            // Exact Blackman window. See https://en.wikipedia.org/wiki/Window_function#Blackman_window
            double a0 = 7938.0 / 18608.0;
            double a1 = 9240.0 / 18608.0;
            double a2 = 1430.0 / 18608.0;
            for (size_t i = 0; i < N; ++i)
            {
                window[i] = (T)(a0 - a1 * std::cos(2 * Pi / N * i) + a2 * std::cos(4 * Pi / N * i));
            }
            return window;
        }
        template <typename T>
        static std::vector<T> NoWindow(size_t N)
        {
            std::vector<T> window;
            window.resize(N);
            for (size_t i = 0; i < N; ++i)
            {
                window[i] = 1.0;
            }
            return window;
        }

        template <typename T>
        static std::vector<T> Hann(int size)
        {
            double alpha = 0.5;
            double beta = 1.0 - alpha;

            std::vector<T> window;
            window.resize(size);
            double scale = LsNumerics::Pi * 2 / (size - 1);
            for (int i = 0; i < size; ++i)
            {
                window[i] = (T)(alpha - beta * std::cos(scale * i));
            }
            return window;
        }
        template <typename T>
        static std::vector<T> HannSquared(int size)
        {
            double alpha = 0.5;
            double beta = 1.0 - alpha;

            std::vector<T> window;
            window.resize(size);
            double scale = LsNumerics::Pi * 2 / (size - 1);
            for (int i = 0; i < size; ++i)
            {
                double v = (alpha - beta * std::cos(scale * i));
                window[i] = (T)(v*v);
            }
            return window;
        }
        template <typename T>
        static std::vector<T> Hamming(int size)
        {
            double alpha = 0.54;
            double beta = 1.0 - alpha;

            std::vector<T> window;
            window.resize(size);

            double scale = LsNumerics::Pi * 2 / (size - 1);
            for (int i = 0; i < size; ++i)
            {
                window[i] = (T)(alpha - beta * std::cos(scale * i));
            }
            return window;
        }
        template <typename T>
        static std::vector<T> Rect(int size)
        {
            std::vector<T> window;
            window.resize(size);

            double scale = LsNumerics::Pi * 2 / (size - 1);
            for (int i = 0; i < size; ++i)
            {
                window[i] = (T)(1.0);
            }
            return window;
        }

        // Poor skirt, but shows prominent peaks for sin waves in source.
        template<typename T> 
        static std::vector<T> FlatTop(int size)
        {
            std::vector<T> window;
            window.resize(size);
            // source https://en.wikipedia.org/wiki/Window_function#:~:text=0Flat%20top%20window
            constexpr double a_0=0.21557895;
            constexpr double a_1=0.41663158;
            constexpr double a_2=0.277263158;
            constexpr double a_3=0.083578947;
            constexpr double a_4=0.006947368;

            double w = LsNumerics::Pi * 2 / (size-1);
            for (int n = 0; n < size; ++n)
            {
                window[n] = a_0 - a_1*std::cos(w*n)+ a_2*std::cos(2*w*n) - a_3*std::cos(3*w*n) + a_4*std::cos(4*w*n);
            }
            return window;
        }
    };

} // namespace