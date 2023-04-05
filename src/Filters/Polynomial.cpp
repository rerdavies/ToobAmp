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

#include "Polynomial.h"
#include <cmath>

using namespace toob;

const Polynomial Polynomial::ONE(1.0);
const Polynomial Polynomial::ZERO;


static void ReduceVector(std::vector<double>*vec)
{
    size_t ix = vec->size();
    while (ix != 0 && vec->at(ix-1) == 0)
    {
        --ix;
    }
    if (ix != vec->size())
    {
        vec->resize(ix);
    }
}
Polynomial Polynomial::operator+(const Polynomial & other)
{
    std::vector<double> result(std::max(this->values.size(),other.values.size()));
    int iMin = std::min(values.size(),other.values.size());
    for (int i = 0; i < iMin; ++i)
    {
        values.push_back(values[i] + other.values[i]);
    }
    if (values.size() > other.values.size())
    {
        for (std::size_t i = iMin; i < values.size(); ++i)
        {
            result.push_back(values[i]);
        }
    } else {
        for (std::size_t i = iMin; i < other.values.size(); ++i)
        {
            result.push_back(other.values[i]);
        }
    }
    ReduceVector(&result);
    return Polynomial(result);

}
Polynomial Polynomial::operator-(const Polynomial & other)
{
    std::vector<double> result(std::max(this->values.size(),other.values.size()));
    int iMin = std::min(values.size(),other.values.size());
    for (int i = 0; i < iMin; ++i)
    {
        values.push_back(values[i] - other.values[i]);
    }
    if (values.size() > other.values.size())
    {
        for (std::size_t i = iMin; i < values.size(); ++i)
        {
            result.push_back(values[i]);
        }
    } else {
        for (std::size_t i = iMin; i < other.values.size(); ++i)
        {
            result.push_back(-other.values[i]);
        }
    }
    ReduceVector(&result);
    return Polynomial(result);

}
Polynomial Polynomial::operator*(const Polynomial & other) {
    std::vector<double> result;
    result.resize(values.size()+other.values.size());

    for (size_t i = 0; i < values.size(); ++i)
    {
        for (size_t j = 0; j < other.values.size(); ++j)
        {
            result[i+j] += values[i]*other.values[j];
        }
    }
    ReduceVector(&result);

    return Polynomial(result);
}

