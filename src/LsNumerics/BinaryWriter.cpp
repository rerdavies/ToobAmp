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

#include "BinaryWriter.hpp"
#include <stdexcept>
#include "../ss.hpp"
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>


using namespace LsNumerics;
using namespace std;
using namespace boost::iostreams;


class BinaryWriter::GZipExtra: public BinaryWriter::Extra {
public:

    GZipExtra(const std::filesystem::path& path)
    :out(&gzipFbuf)
    {
        f.open(path,ios_base::binary | ios_base::trunc);
        if (!f.is_open())
        {
            throw std::logic_error(SS("Can't open file " << path.string()));
        }
        gzipFbuf.push(gzip_compressor());
        gzipFbuf.push(f);

    }
    virtual std::ostream*GetStream() {
        return &out;
    }
private:
    std::ofstream f;
    boost::iostreams::filtering_streambuf<boost::iostreams::output> gzipFbuf;
    std::ostream out;

};
class BinaryWriter::FStreamExtra: public BinaryWriter::Extra {
public:
    FStreamExtra(const std::filesystem::path& path)
    {
        out.open(path,ios_base::out | ios_base::binary | ios_base::trunc);
        if (!out.is_open())
        {
            throw std::logic_error(SS("Path not found: " << path));
        }
    }

    virtual std::ostream*GetStream() {
        return &out;
    }
private:
    std::ofstream out;
};

BinaryWriter::BinaryWriter(const std::filesystem::path &path)
{

    if (path.extension().string() == ".gz")
    {
        pExtra = new GZipExtra(path);
    } else {
        pExtra = new FStreamExtra(path);
    }
    this->pOut = pExtra->GetStream();
}

BinaryWriter::~BinaryWriter()
{
    delete pExtra;
}

BinaryWriter&BinaryWriter::operator<<(std::uint16_t value)
{
    (*pOut) << (char)(value);
    (*pOut) << (char)(value >> 8);
    this->CheckFail();
    return *this;
}
BinaryWriter& BinaryWriter::operator<<(std::int16_t value)
{
    (*pOut) << (char)(value);
    (*pOut) << (char)(value >> 8);
    this->CheckFail();
    return *this;
}
BinaryWriter& BinaryWriter::operator<<(std::int32_t value)
{
    (*this) << (uint16_t)value;
    (*this) << (uint16_t)(value >> 16);
    return *this;
}
BinaryWriter& BinaryWriter::operator<<(std::uint32_t value)
{
    (*this) << (uint16_t)value;
    (*this) << (uint16_t)(value >> 16);
    return *this;
}

BinaryWriter&BinaryWriter::write(size_t size, void*data)
{
    for (size_t i = 0; i < size; ++i)
    {
        (*pOut) << ((char*)data)[i];
    }
    return *this;
}


#pragma GCC diagnostic ignored "-Wstrict-aliasing"

BinaryWriter& BinaryWriter::operator<<(float value)
{
    (*this) << *(uint16_t*)(&value);
    return *this;
}
BinaryWriter& BinaryWriter::operator<<(double value)
{
    (*this) << *(uint64_t*)(&value);
    return *this;

}
BinaryWriter& BinaryWriter::operator<<(const std::complex<double>&value)
{
    (*this) << value.real() << value.imag();
    return *this;
}

BinaryWriter& BinaryWriter::operator<<(const std::string& value)
{
    (*this) << (uint32_t)value.length();
    for (char c: value)
    {
        (*this) << c;
    }
    return *this;
}


BinaryWriter& BinaryWriter::operator<<(std::int64_t value)
{
    (*this) << ((uint32_t)(value)) << ((uint32_t)(value >> 32));
    return *this;
}
BinaryWriter& BinaryWriter::operator<<(std::uint64_t value)
{
    (*this) << ((uint32_t)(value)) << ((uint32_t)(value >> 32));
    return *this;

}


void BinaryWriter::ThrowWriteError()
{
    throw std::logic_error("Write error.");
}