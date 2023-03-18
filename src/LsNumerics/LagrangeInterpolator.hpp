/*
 * MIT License
 *
 * Copyright (c) 2023 Robin E. R. Davies
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

#include <vector>
#include <cstddef>

namespace LsNumerics
{
    class LagrangeInterpolator
    {
    public:
        LagrangeInterpolator(size_t n)
        {
            this->n = n;
            w.resize(n);
            num.resize(n);
            for (size_t i = 0; i < n; ++i)
            {
                w[i] = GetDenominator(i, n);
            }
        }

        double Interpolate(float *values, double x)
        {
            /*
             * The formula:
             *
             *  l(k,x) = PROD (i=0..K-1) of (x-i)  * PROD (i=K+1..N-1) of (x-i) * sample[k]
             *          /  PROD (i=0..K-1) of (k-i)  * PROD (i = K+1..N-1) of (k-i)
             *
             * L(x) = sum(k=0..N-1) of l(k,x)
             *
             * The denominator of l(k,x) is constant, so we pre-compute it.
             *
             * We can compute the denominator quickly by computing running product of the left and right sums.
             */

            // Hard-wired 6 point lagrange interpolator.

            int x0 = (int)(x - n / 2);
            double xFrac = x - x0;
            double accumulator = 1;
            for (ptrdiff_t i = (ptrdiff_t)(n - 1); i >= 0; --i)
            {
                num[i] = accumulator;
                accumulator *= xFrac - i;
            }

            double sum = 0;
            accumulator = 1;
            for (size_t i = 0; i < n; ++i)
            {
                sum += values[x0 + i] * accumulator * num[i] * w[i];
                accumulator *= xFrac - i;
            }
            return sum;
        }
        double Interpolate(const std::vector<float>&values, double x)
        {
            /*
             * The formula:
             *
             *  l(k,x) = PROD (i=0..K-1) of (x-i)  * PROD (i=K+1..N-1) of (x-i) * sample[k]
             *          /  PROD (i=0..K-1) of (k-i)  * PROD (i = K+1..N-1) of (k-i)
             *
             * L(x) = sum(k=0..N-1) of l(k,x)
             *
             * The denominator of l(k,x) is constant, so we pre-compute it.
             *
             * We can compute the denominator quickly by computing running product of the left and right sums.
             */

            // Hard-wired 6 point lagrange interpolator.

            int x0 = (int)(x - n / 2);
            if (x0 < 0 || x0+n >= values.size())
            {
                // check range.
                double xFrac = x - x0;
                double accumulator = 1;
                for (ptrdiff_t i = (ptrdiff_t)(n - 1); i >= 0; --i)
                {
                    num[i] = accumulator;
                    accumulator *= xFrac - i;
                }

                double sum = 0;
                accumulator = 1;
                for (size_t i = 0; i < n; ++i)
                {
                    float yN;
                    ptrdiff_t index = (ptrdiff_t)(x0+i);
                    if (index < 0) 
                    {
                        yN = values[0];
                    } else if (index >= (ptrdiff_t)(values.size()))
                    {
                        yN = 0;
                    } else {
                        yN = values[index];
                    }
                    sum += yN * accumulator * num[i] * w[i];
                    accumulator *= xFrac - i;
                }
                return sum;
            } else {
                // no range checks.
                double xFrac = x - x0;
                double accumulator = 1;
                for (ptrdiff_t i = (ptrdiff_t)(n - 1); i >= 0; --i)
                {
                    num[i] = accumulator;
                    accumulator *= xFrac - i;
                }

                double sum = 0;
                accumulator = 1;
                for (size_t i = 0; i < n; ++i)
                {
                    sum += values[x0 + i] * accumulator * num[i] * w[i];
                    accumulator *= xFrac - i;
                }
                return sum;
            }
        }

    private:
        std::vector<double> w;
        std::vector<double> num;
        size_t n;
        double GetDenominator(ptrdiff_t k, ptrdiff_t n)
        {
            double p = 1;
            for (ptrdiff_t i = 0; i < n; ++i)
            {
                if (i != k)
                {
                    p *= (k - i);
                }
            }
            return 1.0 / p;
        }
    };
}
