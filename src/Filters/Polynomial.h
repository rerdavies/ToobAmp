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
#include <vector>
#include <cstddef>

namespace TwoPlay {
    class Polynomial {
    private:
        std::vector<double> values;

    public:
        static const Polynomial ONE;
        static const Polynomial ZERO;


        Polynomial() {

        }
        Polynomial(std::initializer_list<double> initializerList)
        :values(initializerList)
        {

        }
        Polynomial(int length, double*values)
        :values(length)
        {
            
            for (int i = 0; i < length; ++i)
            {
                this->values.push_back(values[i]);
            }
        }
        Polynomial(std::vector<double> values)
        {
            this->values = values;
        }

        Polynomial(double v0)
        {
            values.push_back(v0);
        }

        Polynomial(double v0, double v1)
        {
            values.push_back(v0);
            values.push_back(v1);
        }
        double & operator[](int index) {
            return values.at(index);
        }

        void Resize(std::size_t size) { values.resize(size);}

        Polynomial operator+(const Polynomial & other);
        Polynomial operator-(const Polynomial & other);
        Polynomial operator*(const Polynomial & other);
        Polynomial& operator=(const Polynomial & other)
        {
            this->values = other.values;
            return *this;
        }

    };
}