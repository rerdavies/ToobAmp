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

#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstddef>
#include <complex>
#include <vector>
#include <string>

namespace LsNumerics {
    class BinaryWriter {
    public:
        BinaryWriter(const std::filesystem::path &path);
        ~BinaryWriter();

        BinaryWriter&operator <<(std::uint8_t value) {
            out << (char)value;
            CheckFail();
            return *this;
        }
        BinaryWriter&operator<<(std::int8_t value) {
            out << (char)value;
            CheckFail();
            return *this;
        }
        BinaryWriter&operator<<(char value) {
            out << (char)value;
            CheckFail();
            return *this;
        }
        BinaryWriter&operator <<(bool value) {
            out << (char)(value ? 1:0);
            CheckFail();
            return *this;
        }
        BinaryWriter&operator<<(std::int16_t value);
        BinaryWriter&operator<<(std::uint16_t value);
        BinaryWriter&operator<<(std::int32_t value);
        BinaryWriter&operator<<(std::uint32_t value);
        BinaryWriter&operator<<(std::int64_t value);
        BinaryWriter&operator<<(std::uint64_t value);

        BinaryWriter&operator<<(float value);
        BinaryWriter&operator<<(double value);
        BinaryWriter&operator<<(const std::complex<double>&value);
        BinaryWriter&operator<<(const std::string& value);

        std::streampos Tell() { return out.tellp();}

        template <typename T> 
        BinaryWriter& operator<<(const std::vector<T>&vector)
        {
            for (size_t i = 0; i < vector.size(); ++i)
            {
                (*this) << vector[i];
            }
            return (*this);
        }

        BinaryWriter&write(size_t size, void*data);


    private:
        void ThrowWriteError();
        void CheckFail()
        {
            if (out.fail())
            {
                ThrowWriteError();
            }
        }
    private:
        std::ofstream out;
    };
}