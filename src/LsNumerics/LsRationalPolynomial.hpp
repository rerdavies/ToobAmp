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

#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "LsPolynomial.hpp"
namespace LsNumerics
{
    class RationalPolynomial
    {
    private:
        Polynomial numerator;
        Polynomial denominator;

    public:
        static const RationalPolynomial One;
        static const RationalPolynomial Zero;

        RationalPolynomial()
        {
        }
        explicit RationalPolynomial(double v)
        :   numerator(Polynomial(v)), 
            denominator(Polynomial::One)
        {
        }
        explicit RationalPolynomial(const Polynomial&polynomial)
        : numerator(polynomial),
            denominator(Polynomial::One)
        {

        }
        RationalPolynomial(const Polynomial&numerator, const Polynomial&denominator)
            :numerator(numerator),
            denominator(denominator)
        {

        }
        RationalPolynomial(Polynomial&& numerator,Polynomial&&denominator) noexcept
            :   numerator(std::forward<Polynomial>(numerator)),
                denominator(std::forward<Polynomial>(denominator))
        {

        }
        RationalPolynomial(const RationalPolynomial& other) noexcept
            :   numerator(other.numerator),
                denominator(other.denominator)
        {
        }
        RationalPolynomial(RationalPolynomial&& other) noexcept
        :   numerator(std::forward<Polynomial>(other.numerator)),
            denominator(std::forward<Polynomial>(other.denominator))
        {
        }
        const Polynomial&Numerator() const { return numerator; }
        const Polynomial&Denominator() const { return denominator; }

        bool IsZero() const {
            return numerator.IsZero();
        }
        bool IsOne() const {
            return numerator == denominator;
        }


        static RationalPolynomial Add(const RationalPolynomial &left, const RationalPolynomial right)
        {
            return RationalPolynomial(
                left.numerator * right.denominator + right.numerator*left.denominator,
                left.denominator*right.denominator
            );
        }


        static RationalPolynomial Subtract(const RationalPolynomial& left, const RationalPolynomial&right)
        {
            return RationalPolynomial(
                left.numerator * right.denominator - right.numerator*left.denominator,
                left.denominator*right.denominator
            );
        }


        static RationalPolynomial Multiply(double left, const RationalPolynomial& right)
        {
            return RationalPolynomial(
                right.numerator*left,
                right.denominator
            );
        }
        static RationalPolynomial Multiply(const RationalPolynomial& left, double right)
        {
            return Multiply(right,left);
        }

        static RationalPolynomial Multiply(const RationalPolynomial & left, const RationalPolynomial& right)
        {
            return RationalPolynomial(
                left.numerator*right.numerator,
                left.denominator*right.denominator
            );
        }
        double At(double x) const
        {
            return numerator.At(x)/denominator.At(x);
        }
        bool Equals(const RationalPolynomial& other) const
        {
            return numerator == other.numerator && denominator == other.denominator;
        }

        RationalPolynomial& operator=(const RationalPolynomial& other) noexcept
        {
            numerator = other.numerator;
            denominator = other.denominator;
            return *this;
        }
        bool operator ==(const RationalPolynomial &right) const
        {
            return Equals(right);
        }
        bool operator !=(const RationalPolynomial& right) const
        {
            return !Equals(right);
        }



    };

    inline RationalPolynomial operator+(const RationalPolynomial& left, const RationalPolynomial& right)
    {
        return RationalPolynomial::Add(left, right);
    }
    inline RationalPolynomial operator+(const RationalPolynomial& left, double right)
    {
        return RationalPolynomial::Add(left, RationalPolynomial(right));
    }
    inline RationalPolynomial operator+(double left, const RationalPolynomial& right)
    {
        return RationalPolynomial::Add(RationalPolynomial(left), right);
    }
    inline RationalPolynomial operator-(const RationalPolynomial& left, const RationalPolynomial& right)
    {
        return RationalPolynomial::Subtract(left, right);
    }
    inline RationalPolynomial operator-(const RationalPolynomial& left, double right)
    {
        return RationalPolynomial::Subtract(left, RationalPolynomial(right));
    }
    inline RationalPolynomial operator-(double left, const RationalPolynomial& right)
    {
        return RationalPolynomial::Subtract(RationalPolynomial(left), right);
    }
    inline RationalPolynomial operator*(const RationalPolynomial& left, const RationalPolynomial& right)
    {
        return RationalPolynomial::Multiply(left, right);
    }
    inline RationalPolynomial operator*(const RationalPolynomial& left, double right)
    {
        return RationalPolynomial::Multiply(left, right);
    }
    inline RationalPolynomial operator*(double left, const RationalPolynomial& right)
    {
        return RationalPolynomial::Multiply(left, right);
    }

};

