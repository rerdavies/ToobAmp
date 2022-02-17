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

#include <cstdint>
#include <vector>
#include <cstddef>

namespace LsNumerics
{
    class Polynomial
    {
    private:
        std::vector<double> values;
    public:
        Polynomial()
        {
        }
        Polynomial(double v)
        :values({v})
        {
        }
        Polynomial(std::initializer_list<double> initializer)
            :values(initializer)
        {

        }
        Polynomial(Polynomial&& other) noexcept
            :values(std::forward<std::vector<double>>(other.values))
        {

        }
        Polynomial(const Polynomial& other) noexcept
            :values(other.values)
        {
        }
        Polynomial(std::vector<double>&& values) noexcept
            :values(std::forward<std::vector<double> >(values))
        {

        }
        Polynomial(const std::vector<double>& values) noexcept
        {
            this->values = values;
        }
        double operator[](int index) const {
            return values[index];
        }
        double operator[](size_t index) const {
            return values[index];
        }
        //double& operator[](int index) {
        //    return values[index];
        //}


        static const Polynomial One;
        static const Polynomial Zero;

        size_t Size() const {
            return values.size();
        }

        bool IsZero() const {
            return Size() == 0;
        }
        bool IsOne() const {
            return Size() == 1 && values[0] == 1;
        }

        static Polynomial Add(const Polynomial &left, double right)
        {
            if (right == 0) return left;
            if (left.values.size() == 0) return Polynomial(right);
            if (left.values.size() == 1 && left.values[0] == -right)
            {
                return Polynomial::Zero;
            }

            size_t length = left.values.size();
            std::vector<double> values = left.values;
            values[0] += right;
            return Polynomial(std::move(values));
        }

        Polynomial Derivative()
        {
            if (this->values.size() == 0)
            {
                return Zero;
            }
            std::vector<double> values;
            values.resize(this->values.size() - 1);

            for (size_t i = 1; i < this->values.size(); ++i)
            {
                values[i-1] = (this->values[i] * i);
            }
            return Polynomial(std::move(values));
        }

        static Polynomial Add(double left, Polynomial right)
        {
            return Add(left, right);
        }
        static Polynomial Subtract(Polynomial left, double right)
        {
            if (right == 0) return left;
            if (left.values.size() == 0) return Polynomial(-right);
            if (left.values.size() == 1 && left.values[0] == right)
            {
                return Polynomial::Zero;
            }

            size_t length = left.values.size();
            std::vector<double> values(length);
            for (size_t i = 0; i < length; ++i)
            {
                values.push_back(left.values[i]);
            }
            values[0] -= right;
            return Polynomial(std::move(values));
        }
        static Polynomial Subtract(double left, Polynomial right)
        {
            if (right.values.size() == 0) return left;
            if (right.values.size() == 1 && right.values[0] == left)
            {
                return Polynomial::Zero;
            }

            size_t length = right.values.size();
            std::vector<double> values(length);

            for (size_t i = 0; i < length; ++i)
            {
                values.push_back(-right.values[i]);
            }
            values[0] += left;
            return Polynomial(values);
        }

        static Polynomial Add(const Polynomial &left, const Polynomial &right)
        {
            if (left.values.size() == right.values.size())
            {
                size_t length = left.values.size();
                while (length != 0 && left.values[length - 1] == -right.values[length - 1])
                {
                    --length;
                }
                std::vector<double> values(length);
                values.resize(length);

                for (size_t i = 0; i < length; ++i)
                {
                    values[i] = (left.values[i] + right.values[i]);
                }
                return Polynomial(std::move(values));
            }
            else
            {
                size_t length = std::max(left.values.size(), right.values.size());
                size_t shared = std::min(left.values.size(), right.values.size());

                std::vector<double> values(length);
                values.resize(length);

                for (size_t i = 0; i < shared; ++i)
                {
                    values[i] = (left.values[i] + right.values[i]);
                }
                for (size_t i = shared; i < left.values.size(); ++i)
                {
                    values[i] = (left.values[i]);
                }
                for (size_t i = shared; i < right.values.size(); ++i)
                {
                    values[i] = (right.values[i]);
                }
                return Polynomial(std::move(values));
            }
        }

        static Polynomial Subtract(const Polynomial& left, const Polynomial& right)
        {
            if (left.values.size() == right.values.size())
            {
                size_t length = left.values.size();
                while (length != 0 && left.values[length - 1] == right.values[length - 1])
                {
                    --length;
                }
                std::vector<double> values(length);
                values.resize(length);

                for (size_t i = 0; i < length; ++i)
                {
                    values[i] = (left.values[i] - right.values[i]);
                }
                return Polynomial(std::move(values));
            }
            else
            {
                size_t length = std::max(left.values.size(), right.values.size());
                size_t shared = std::min(left.values.size(), right.values.size());

                std::vector<double> values(length);
                values.resize(length);

                for (size_t i = 0; i < shared; ++i)
                {
                    values[i] = (left.values[i] - right.values[i]);
                }
                for (size_t i = shared; i < left.values.size(); ++i)
                {
                    values[i] = (left.values[i]);
                }
                for (size_t i = shared; i < right.values.size(); ++i)
                {
                    values[i] = (-right.values[i]);
                }
                return Polynomial(std::move(values));
            }
        }

        static Polynomial Multiply(double left, const Polynomial& right)
        {
            if (left == 0) return Polynomial::Zero;

            size_t length = right.values.size();

            std::vector<double> values(length);
            values.resize(length);

            for (size_t i = 0; i < length; ++i)
            {
                values[i] = left * right[i];
            }
            return Polynomial(std::move(values));
        }
        static Polynomial Multiply(const Polynomial& left, double right)
        {
            if (right == 0) return Polynomial::Zero;

            size_t length = left.values.size();
            std::vector<double> values(length);
            values.resize(length);
            for (size_t i = 0; i < length; ++i)
            {
                values[i] = left[i] * right;
            }
            return Polynomial(std::move(values));
        }

        static Polynomial Multiply(const Polynomial & left, const Polynomial& right)
        {
            size_t length = left.values.size()+ right.values.size()- 1;
            std::vector<double> values(length);
            values.resize(length);

            for (size_t i = 0; i < left.values.size(); ++i)
            {
                for (size_t j = 0; j < right.values.size(); ++j)
                {
                    values[i + j] += left.values[i] * right.values[j];
                }
            }
            return Polynomial(std::move(values));
        }
        double At(double x) const
        {
            if (values.size() == 0) return 0;
            double sum = values[0];
            double xP = x;
            for (size_t i = 1; i < values.size(); ++i)
            {
                sum += values[i] * xP;
                xP *= x;
            }
            return sum;
        }
        bool Equals(const Polynomial& other) const
        {
            if (values.size() != other.values.size()) return false;
            size_t length = values.size();
            for (size_t i = 0; i < length; ++i)
            {
                if (values[i] != other.values[i]) return false;
            }
            return true;
        }

        Polynomial& operator=(const Polynomial& other) noexcept
        {
            this->values = other.values;
            return *this;
        }
        Polynomial& operator=(Polynomial&& other) noexcept
        {
            this->values = std::move(other.values);
            return *this;
        }
        bool operator ==(const Polynomial &right) const
        {
            return Equals(right);
        }
        bool operator !=(const Polynomial& right) const
        {
            return !Equals(right);
        }

        Polynomial& operator+=(const Polynomial& other)
        {
            if (values.size() == other.values.size())
            {
                size_t length = this->values.size();
                while (length != 0 && this->values[length - 1] == -other.values[length - 1])
                {
                    --length;
                }
                this->values.resize(length);

                for (size_t i = 0; i < length; ++i)
                {
                    this->values[i] = (this->values[i] + other.values[i]);
                }
                return *this;
            }
            else
            {
                size_t length = std::max(this->values.size(), other.values.size());
                size_t shared = std::min(this->values.size(), other.values.size());

                this->values.resize(length);

                for (size_t i = 0; i < shared; ++i)
                {
                    this->values[i] = (this->values[i] + other.values[i]);
                }
                for (size_t i = shared; i < other.values.size(); ++i)
                {
                    this->values[i] = other.values[i];
                }
                return *this;
            }

        }



    };

    inline Polynomial operator+(const Polynomial& left, const Polynomial& right)
    {
        return Polynomial::Add(left, right);
    }
    inline Polynomial operator+(const Polynomial& left, double right)
    {
        return Polynomial::Add(left, right);
    }
    inline Polynomial operator+(double left, const Polynomial& right)
    {
        return Polynomial::Add(left, right);
    }
    inline Polynomial operator-(const Polynomial& left, const Polynomial& right)
    {
        return Polynomial::Subtract(left, right);
    }
    inline Polynomial operator-(const Polynomial& left, double right)
    {
        return Polynomial::Subtract(left, right);
    }
    inline Polynomial operator-(double left, const Polynomial& right)
    {
        return Polynomial::Subtract(left, right);
    }
    inline Polynomial operator*(const Polynomial& left, const Polynomial& right)
    {
        return Polynomial::Multiply(left, right);
    }
    inline Polynomial operator*(const Polynomial& left, double right)
    {
        return Polynomial::Multiply(left, right);
    }
    inline Polynomial operator*(double left, const Polynomial& right)
    {
        return Polynomial::Multiply(left, right);
    }

};

