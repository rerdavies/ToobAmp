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

#include "LsPolynomial.hpp"

namespace LsNumerics
{
    class ChebyshevPolynomial
    {
    private:
        static const Polynomial T0;
        static const Polynomial T1;

        /// <summary>
        /// Create a Chebyshev polynomial of the 1st kind.
        /// </summary>
        /// <param name="n">Order</param>
        /// <returns>Chebyshev polynomial of order N</returns>
    public:
        static Polynomial Tn(int n)
        {
            if (n == 0) return T0;
            if (n == 1) return T1;
            Polynomial nMinus1 = T0;
            Polynomial result = T1;
            Polynomial scale{ 0,2 }; // 2x

            for (int i = 2; i <= n; ++i)
            {
                auto t = scale * result - nMinus1;
                nMinus1 = result;
                result = t;
            }
            return result;
        }

    };
};
