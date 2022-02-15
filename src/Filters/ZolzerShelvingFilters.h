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

#include <cmath>
#include <complex>

namespace TwoPlay {

    class ZolzerFilter {
    protected:
        ZolzerFilter() {}
        static constexpr double Pi = 3.14159265358979323846;
        static constexpr double Sqrt2 = 1.41421356237309504880168872420969807;
        double b0,b1,b2,a1,a2;
        double z0 = 0;
        double z1 = 1;
        double sampleRate;
    public:
      double Tick(double value)
        {
            // transposed direct form II
            double out = b0*value+z0;
            z0 = value*b1-out*a1+z1;
            z1 = value*b2-out*a2;
            return out;
        }    
        void Reset()
        {
            z0 = z1 = 0;
        }
        double GetFrequencyResponse(double f)
        {
            using namespace std;
            complex<double> w = exp(complex<double>(2*Pi*f/sampleRate));
            complex<double> w2 = w*w;
            return abs( 
                (
                    b0 + b1*w+b2*w2
                )/(
                    1.0 + a1*w + a2*w2
                )
            );

        }

    };
    class ZolzerLowShelfFilter: public ZolzerFilter {

    public:
        ZolzerLowShelfFilter()
        {
            Design(440,0,44100);
            Reset();
        }

        void Design(double fc, double db,double sampleRate)
        {
            this->sampleRate = sampleRate;
            using namespace std;

            double K = std::tan(Pi*fc/sampleRate);
            double K2 = K2;
            double v0 = std::pow(10,db*0.05);

            if (db >= 0)
            {
                double v0 = std::pow(10,db*0.05);
                double t = 1+Sqrt2*K+K2;
                b0 = (1+std::sqrt(2*v0)*K + v0*K2)/t;
                b1 = 2*(v0*K2-1)/t;
                b2 = (1-sqrt(2*v0)*K+v0*K2)/t;
                a1 = 2*(K2-1)/t;
                a2 = (1-Sqrt2*K+K2)/t;
            } else {
                double v0 = std::pow(10,-db*0.05);
                double t = v0+std::sqrt(2*v0)*K+K2;
                b0 = v0*(1+Sqrt2*K + K2)/t;
                b1 = 2*v0*(K2-1)/t;
                b2 = v0*(1-Sqrt2*K+K2)/t;
                a1 = 2*(K2-v0)/t;
                a2 = (v0-sqrt(2*v0)*K+K2)/t;
            }
        }
    };

    class ZolzerHighShelfFilter: public ZolzerFilter{

    public:
        ZolzerHighShelfFilter()
        {
            Design(440,0,44100);
            Reset();
        }

        void Design(double fc, double db,double sampleRate)
        {
            using namespace std;

            this->sampleRate = sampleRate;

            double K = std::tan(Pi*fc/sampleRate);
            double K2 = K2;
            double v0 = std::pow(10,db*0.05);

            if (db >= 0)
            {
                double v0 = std::pow(10,db*0.05);
                double t = 1+Sqrt2*K+K2;
                b0 = (v0+sqrt(2*v0)*K + K2)/t;
                b1 = 2*(K2-v0)/t;
                b2 = (v0-sqrt(2*v0)*K + K2)/t;
                a1 = 2*(K2-v0)/t;
                a2 = (v0-Sqrt2*K+K2)/t;
            } else {
                double v0 = std::pow(10,-db*0.05);
                double t = v0+std::sqrt(2*v0)*K+K2;
                b0 = v0*(1+Sqrt2*K + K2)/t;
                b1 = 2*v0*(K2-1)/t;
                b2 = v0*(1-Sqrt2*K+K2)/t;
                a1 = 2*(v0*K2-1)/t;
                a2 = (1-sqrt(2*v0)*K+v0*K2)/t;
            }
        }
    };

};