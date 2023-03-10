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
#include <vector>
#include <cstddef>
#include <assert.h>

namespace LsNumerics
{

    // FixedPolynomial<N> -- Polynomial with no heap allocations, suitable for use in realtime.
    template <size_t N>
    class FixedPolynomial
    {
    private:
        size_t size;
        double values[N];

        void AssertSize(size_t size)
        {
            assert(size <= N);
        }
        void Reduce() {
            while (size > 0 && values[size-1] == 0)
            {
                --size;
            }
        }
    public:
        constexpr static size_t _N = N;

        FixedPolynomial()
        {
            size = 0;
        }
        FixedPolynomial(double v)
        {
            if (v == 0) 
            {
                size = 0;
            } else {
                size = 1;
                values[0] = v;
            }
        }
        FixedPolynomial(std::initializer_list<double> initializer)
        {
            size = 0;
            for (double i: initializer)
            {
                values[size++] = i;
                AssertSize(size);
            }
            Reduce();
        }
        template <size_t M>
        FixedPolynomial(const FixedPolynomial<M>& other) noexcept
        {
            AssertSize(other.size);
            this->size = other.size;
            for (int i = 0; i < this->size; ++i)
            {
                this->values[i] = other.values[i];
            }
        }
        FixedPolynomial(const std::vector<double>& values) noexcept
        {
            size_t size = size;
            AssertSize(size);
            this->size = size;
            for (size_t i = 0; i < size; ++i)
            {
                this->values[i] = values[i];
            }
            Reduce();
        }
        double operator[](int index) const {
            if ((size_t)index >= size) return 0;
            return values[index];
        }
        double operator[](size_t index) const {
            if (index >= size) return 0;
            return values[index];
        }
        //double& operator[](int index) {
        //    return values[index];
        //}

        size_t Size() const {
            return size;
        }

        bool IsZero() const {
            return size == 0;
        }
        bool IsOne() const {
            return size == 1 && values[0] == 1;
        }

        static FixedPolynomial<N> Add(const FixedPolynomial<N> &left, double right)
        {
            FixedPolynomial<N> result;
            result.size = left.size;
            double*values = result.values;
            if (result.size == 0)
            {
                values[0] = right;
                result.size = 1;
            } else {
                for (size_t i = 0; i < result.size; ++i)
                {
                    values[i] = left.values[i];
                }
                result[0] += right;
                if (result.size == 1 && result[0] == 0)
                {
                    result.size = 0;
                }
            }
            return result;
        }

        FixedPolynomial<N> Derivative()
        {
            if (this->size == 0)
            {
                return FixedPolynomial<N>();
            }
            FixedPolynomial<N> result;
            result.size = size-1;

            for (size_t i = 1; i < this->size; ++i)
            {
                result.values[i-1] = (this->values[i] * i);
            }
            return result;
        }

        static FixedPolynomial<N> Add(double left, FixedPolynomial<N> right)
        {
            return Add(left, right);
        }
        static FixedPolynomial<N> Subtract(FixedPolynomial<N> left, double right)
        {
            FixedPolynomial<N> result;
            result.size = left.size;
            double *values = result.values;
            if (result.size == 0)
            {
                values[0] = -right;
                result.size = 1;
            } else {
                result.values[0] = left.values[0]-right;
                for (size_t i = 1; i < result.size; ++i)
                {
                    values[i] = left.values[i];
                }
                if (result.size == 1 && values[0] == 0)
                {
                    result.size = 0;
                }
            }
            return result;
        }
        static FixedPolynomial<N> Subtract(double left, FixedPolynomial<N> right)
        {
            FixedPolynomial<N> result;
            size_t length = right.size;

            if (length == 0) {
                result.size = 1;
                result.values[0] = left;

            } else {
                result.values[0] = left-right.values[0];
                for (size_t i = 0; i < length; ++i)
                {
                    result.values[i] = -right.values[i];
                }
                if (length == 1 && result.values[0] == 0)
                {
                    result.size = 0;
                }
            }
            return result;
        }

        static FixedPolynomial<N> Add(const FixedPolynomial<N> &left, const FixedPolynomial<N> &right)
        {
            size_t length = std::max(left.size, right.size);
            size_t shared = std::min(left.size, right.size);
            FixedPolynomial<N> result;

            double*values = result.values;
            result.size = length;

            for (size_t i = 0; i < shared; ++i)
            {
                values[i] = (left.values[i] + right.values[i]);
            }
            for (size_t i = shared; i < left.size; ++i)
            {
                values[i] = (left.values[i]);
            }
            for (size_t i = shared; i < right.size; ++i)
            {
                values[i] = (right.values[i]);
            }
            result.Reduce();
            return result;
        }

        static FixedPolynomial<N> Subtract(const FixedPolynomial<N>& left, const FixedPolynomial<N>& right)
        {
            size_t length = std::max(left.size, right.size);
            size_t shared = std::min(left.size, right.size);
            FixedPolynomial<N> result;

            double*values = result.values;
            result.size = length;

            for (size_t i = 0; i < shared; ++i)
            {
                values[i] = (left.values[i] - right.values[i]);
            }
            for (size_t i = shared; i < left.size; ++i)
            {
                values[i] = (left.values[i]);
            }
            for (size_t i = shared; i < right.size; ++i)
            {
                values[i] = -(right.values[i]);
            }
            result.Reduce();
            return result;

        }

        static FixedPolynomial<N> Multiply(double left, const FixedPolynomial<N>& right)
        {
            if (left == 0) return FixedPolynomial<N>();

            size_t length = right.size;
            FixedPolynomial<N> result;
            result.size = right.size;
            double*values = result.values;

            for (size_t i = 0; i < length; ++i)
            {
                values[i] = left * right[i];
            }
            return result;
        }
        static FixedPolynomial<N> Multiply(const FixedPolynomial<N>& left, double right)
        {
            return Multiply(right,left);
        }

        static FixedPolynomial<N> Multiply(const FixedPolynomial<N> & left, const FixedPolynomial<N>& right)
        {
            size_t length = left.size+ right.size- 1;

            FixedPolynomial<N> result;
            result.AssertSize(length);
            result.size = length;
            double*values = result.values;
            for (size_t i = 0; i < length; ++i)
            {
                values[i] = 0;
            }

            for (size_t i = 0; i < left.size; ++i)
            {
                for (size_t j = 0; j < right.size; ++j)
                {
                    values[i + j] += left.values[i] * right.values[j];
                }
            }
            return result;
        }
        double At(double x) const
        {
            if (size == 0) return 0;
            double sum = values[0];
            double xP = x;
            for (size_t i = 1; i < size; ++i)
            {
                sum += values[i] * xP;
                xP *= x;
            }
            return sum;
        }
        bool Equals(const FixedPolynomial<N>& other) const
        {
            if (size != other.size) return false;
            size_t length = size;
            for (size_t i = 0; i < length; ++i)
            {
                if (values[i] != other.values[i]) return false;
            }
            return true;
        }

        FixedPolynomial<N>& operator=(const FixedPolynomial<N>& other) noexcept
        {
            this->size = other.size;
            for (size_t i = 0; i < this->size; ++i)
            {
                this->values[i] = other.values[i];
            }
            return *this;
        }
        bool operator ==(const FixedPolynomial<N> &right) const
        {
            return Equals(right);
        }
        bool operator !=(const FixedPolynomial<N>& right) const
        {
            return !Equals(right);
        }

        FixedPolynomial<N>& operator+=(const FixedPolynomial<N>& other)
        {
            size_t length = std::max(this->size, other.size);
            size_t shared = std::min(this->size, other.size);

            this->values.resize(length);

            for (size_t i = 0; i < shared; ++i)
            {
                this->values[i] += other.values[i];
            }
            for (size_t i = shared; i < other.size; ++i)
            {
                this->values[i] = other.values[i];
            }
            this->size = length;
            Reduce();
            return *this;
        }



    };

    template<size_t N>
    inline FixedPolynomial<N> operator+(const FixedPolynomial<N>& left, const FixedPolynomial<N>& right)
    {
        return FixedPolynomial<N>::Add(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator+(const FixedPolynomial<N>& left, double right)
    {
        return FixedPolynomial<N>::Add(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator+(double left, const FixedPolynomial<N>& right)
    {
        return FixedPolynomial<N>::Add(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator-(const FixedPolynomial<N>& left, const FixedPolynomial<N>& right)
    {
        return FixedPolynomial<N>::Subtract(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator-(const FixedPolynomial<N>& left, double right)
    {
        return FixedPolynomial<N>::Subtract(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator-(double left, const FixedPolynomial<N>& right)
    {
        return FixedPolynomial<N>::Subtract(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator*(const FixedPolynomial<N>& left, const FixedPolynomial<N>& right)
    {
        return FixedPolynomial<N>::Multiply(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator*(const FixedPolynomial<N>& left, double right)
    {
        return FixedPolynomial<N>::Multiply(left, right);
    }
    template<size_t N>
    inline FixedPolynomial<N> operator*(double left, const FixedPolynomial<N>& right)
    {
        return FixedPolynomial<N>::Multiply(left, right);
    }

}

