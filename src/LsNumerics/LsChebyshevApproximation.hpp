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
#include "LsChebyshevPolynomial.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include "LsMath.hpp"



namespace LsNumerics
{
    using namespace std;

    class ChebyshevApproximation
    {
    private:
        double minX, maxX;
        Polynomial polynomial;
        Polynomial derivativePolynomial;
        double xToUSlope;

        double XtoU(double x) const
        {
            return (2 * x - minX - maxX) * xToUSlope;
        }
        double DerivativeScale() const
        {
            return 2 / (maxX - minX);
        }

        double FromU(double u) const
        {
            return ((maxX - minX) * u + maxX + minX) * 0.5;
        }
    public:
        ChebyshevApproximation(const std::function<double(double)>* originalFunction, double minX, double maxX, int N);

        const double PI = LsNumerics::Pi;

        ChebyshevApproximation(double minX, double maxX, const Polynomial& polynomial, const Polynomial& derivativePolynomial)
            : minX(minX),
            maxX(maxX),
            polynomial(polynomial),
            derivativePolynomial(derivativePolynomial)
        {
            xToUSlope = 1.0 / (maxX - minX);

        }

        ChebyshevApproximation(const std::function<double(double)> *originalFunction, const std::function<double(double)>* originalFunctionDerivative, double minX, double maxX, int N)
        {

            this->minX = minX;
            this->maxX = maxX;

            xToUSlope = 1.0 / (maxX - minX);

            polynomial = GetApproximatingPolynomial(originalFunction, N);
            if (originalFunctionDerivative == nullptr || !*originalFunction)
            {
                derivativePolynomial = polynomial.Derivative() * DerivativeScale();
            }
            else {
                derivativePolynomial = GetApproximatingPolynomial(originalFunctionDerivative, N);
            }
        }
    
        Polynomial GetApproximatingPolynomial(const std::function<double(double)>* originalFunction,int N)
        {
            Polynomial result = Polynomial::Zero;
            for (int n = 0; n < N; ++n)
            {
                Polynomial tn = ChebyshevPolynomial::Tn(n);

                double cN = 0;
                for (int k = 1; k <= N; ++k)
                {
                    double u = std::cos(PI * (k - 0.5) / N);
                    double x = FromU(u);
                    double y = (*originalFunction)(x);
                    cN += y * std::cos(PI * n * (k - 0.5) / N);
                }
                if (n != 0)
                {
                    cN = 2 * cN / N;
                }
                else
                {
                    cN = cN / N;
                }

                result += cN * tn;
            }
            return result;
        }

        double At(double x) const
        {
            double u = XtoU(x);
            return polynomial.At(u);
        }


        double DerivativeAt(double x) const
        {
            double u = XtoU(x);
            return derivativePolynomial.At(u);
        }

    private:
        void WritePolynomialInitializer(std::ostream& s, const Polynomial & polynomial)
        {
            s << "{";
            for (size_t i = 0; i < polynomial.Size(); ++i)
            {
                if (i != 0) s << ", ";
                s << polynomial[i];
            }
            s << "}";
        }
    public:
        void WriteInitializer(std::ostream& s)
        {
            s << "{" << minX << ", " << maxX << ", ";
            WritePolynomialInitializer(s, this->polynomial);
            s << ", ";
            WritePolynomialInitializer(s, this->derivativePolynomial);
            
            s << "}";
        }
    };
}

