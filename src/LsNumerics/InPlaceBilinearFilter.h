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

#include <complex>
#include "LsFixedPolynomial.hpp"

namespace LsNumerics {

    template <int N>
    class InPlaceBilinearFilter {
        
    public:
        InPlaceBilinearFilter() {
            for (int i = 0;i < N; ++i)
            {
                bilinearTransform[i] = bilinearTransformStorage+N*i;
            }
            InitTransform(48000);
        }
        void Reset()
        {
            for (int i =0; i < N; ++i)
            {
                this->Z[i] = 0;
            }
        }
    protected:
        // Use Tustin's method (no frequency warping)
        void InitTransform(double sampleRate)
        {
            InitTransform(sampleRate,0,0);
        }
        // Bilinear transform (with frequency warping)

        void InitTransform(double sampleRate, double frequencyS, double frequencyZ)
        {
            

            this->sampleRate = sampleRate;
            for (int i = 0;i < N; ++i)
            {
                B[i] = 0;
                A[i] = 0;
            }

            // zero out the transforms.
            for (int i = 0; i < sizeof(bilinearTransformStorage)/sizeof(bilinearTransformStorage[0]); ++i)
            {
                bilinearTransformStorage[i] = 0;
            }
            // s = 2*sampleRate(1-z^-1)/(1+z^-1)
            double bilinearScale;
            
            if (frequencyS == 0)
            {
                bilinearScale = 2*sampleRate; // tustin's method.
            } else {
                // bilinear transform with frequency warping.
                bilinearScale = 2*Pi*frequencyS/std::tan(2*Pi*frequencyZ/(sampleRate*2));
            }
            FixedPolynomial<N> zNum = bilinearScale*FixedPolynomial<N>({1.0,-1.0});
            FixedPolynomial<N> zDenom{1.0,1.0};


            FixedPolynomial<N> zPolynomial;

            // compute transform from s space to z^-1 space.
            for (int i = 0; i < N; ++i)
            {
                zPolynomial = 1;
                for (int j = 0; j < i; ++j)
                {
                    zPolynomial = zPolynomial*zNum;
                }
                for (int j = i; j < N-1; ++j)
                {
                    zPolynomial = zPolynomial * zDenom;
                }
                for (int j = 0; j < N; ++j)
                {
                    this->bilinearTransform[j][i] += zPolynomial[j];
                }
            }
        }
    public:
        double GetSampleRate() const { return sampleRate; }

        double Tick(double input)
        {
            // Transposed Direct Form II.
            double output = input*B[0] + Z[0];

            Z[N-1] = B[N-1]*input-A[N-1]*output;
            for (int n = 1; n < N-1; ++n)
            {
                double v = B[n]*input + Z[n] -A[n]*output;
                Z[n-1] = v;
            }
            double v = B[N-1]*input -A[N-1]*output;
            Z[N-2] = v;

            return output;
        }
        double GetFrequencyResponse(double frequency) const {
            using namespace std;

            complex<double> w = exp(complex<double>(0,2 * Pi * frequency / sampleRate));


            complex<double> num {B[0]};
            complex<double> denom { 1,0 };

            complex<double> x = w;

            for (int i = 1; i < N; ++i)
            {
                num +=  x*B[i];
                denom += x*A[i];
                x *= w;
            }
            return abs(num/denom);
        }
    private:
        static constexpr double Pi = 3.14159265358979323846;
        
        double sampleRate = -1;
        double bilinearTransformStorage[N*N];
        double *bilinearTransform[N];
        double A[N];
        double B[N];
        double Z[N];

        // Set the s->z transform to a bilinear transform, which maps frequenceS in 
        // s-space of the prototype to frequencyZ in the final filter.

    protected:
        void SetSTransform(double a[N], double b[N])
        {
            for (int r = 0; r < N; ++r)
            {
                double sumA = 0;
                double sumB = 0;
                for (int c = 0; c < N; ++c)
                {
                    sumA += a[c]*bilinearTransform[r][c];
                    sumB += b[c]*bilinearTransform[r][c];
                }
                A[r] = sumA;
                B[r] = sumB;
            }
            double scale = 1/A[0];
            B[0] *= scale;
            A[0] = 1;
            for (int n = 1; n < N; ++n)
            {
                A[n] *= scale;
                B[n] *= scale;
            }
        }

    };


} //namespace