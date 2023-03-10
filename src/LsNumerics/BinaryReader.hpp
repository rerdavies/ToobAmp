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

#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstddef>
#include <complex>
#include <vector>
#include <string>

namespace LsNumerics {
    class BinaryReader {
    public:
        BinaryReader(const std::filesystem::path&path);

        BinaryReader&operator>>(int8_t &value)
        {
            char c;
            in.get(c);
            value = (int8_t)c;
            CheckFail();
            return *this;
        }

        BinaryReader&operator>>(uint8_t &value)
        {
            char c;
            in.get(c);
            value = (uint8_t)c;
            CheckFail();
            return *this;
        }
        BinaryReader& operator>>(bool &value)
        {
            char c;
            in.get(c);
            CheckFail();
            value = c != 0;
            return *this;
        }

        BinaryReader& operator>>(char &value)
        {
            char c;
            in.get(c);
            value = c;
            CheckFail();
            return *this;
        }

        BinaryReader& operator>>(int16_t &value);
        BinaryReader& operator>>(uint16_t &value);
        BinaryReader& operator>>(int32_t &value);
        BinaryReader& operator>>(uint32_t &value);
        BinaryReader& operator>>(int64_t &value);
        BinaryReader& operator>>(uint64_t &value);

        BinaryReader& operator>>(float &value);
        BinaryReader& operator>>(double &value);
        BinaryReader& operator>>(std::complex<double>&value);
        BinaryReader& operator>>(std::string& value);

        std::streampos Tell() { return in.tellg();}


    private:
        void ThrowReadError();
        void CheckFail() {
            if (in.fail())
            {
                ThrowReadError();
            }

        }

        std::ifstream in;
    };
}
