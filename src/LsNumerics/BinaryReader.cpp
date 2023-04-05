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

#include "BinaryReader.hpp"
#include "../ss.hpp"
#include "../util.hpp"
#include <sstream>
#include <stdexcept>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

using namespace LsNumerics;
using namespace std;
using namespace boost::iostreams;
using namespace toob;

struct BinaryReader::Extra
{
    virtual ~Extra() {}
};


struct BinaryReader::GzipExtra : public BinaryReader::Extra
{
public:
    GzipExtra(const std::filesystem::path &path)
    {
        file.open(path, ios_base::in | ios_base::binary);
        if (!file.is_open())
        {
            throw std::logic_error(SS("Can't open file " << path.string()));
        }
        in.push(gzip_decompressor());
        in.push(file);
        pStream = new std::istream(&in);
    }
    ~GzipExtra()
    {
        delete pStream;
    }
    std::istream *Stream() { return pStream; }

private:
    std::ifstream file;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;

    std::istream *pStream = nullptr;
};

struct BinaryReader::FStreamExtra : public BinaryReader::Extra
{
public:
    FStreamExtra(const std::filesystem::path &path)
    {
        f.open(path, ios_base::in | ios_base::binary | ios_base::trunc);
        if (!f.is_open())
        {
            throw std::logic_error(SS("Can't open file " << path.string()));
        }
    }
    std::istream *Stream() { return &f; }

private:
    ifstream f;
};

BinaryReader::~BinaryReader()
{
    if (pExtra)
    {
        delete pExtra;
    }
}

void BinaryReader::ThrowReadError()
{
    throw std::logic_error("Read error.");
}


static std::string gzSuffix("gz");

BinaryReader::BinaryReader(const std::filesystem::path &path)
{
    if (endsWith(path.string(),gzSuffix))
    {
        auto pExtra = new GzipExtra(path);
        ;
        this->pExtra = pExtra;
        this->pIn = pExtra->Stream();
    }
    else
    {
        auto pExtra = new FStreamExtra(path);
        this->pExtra = pExtra;
        this->pIn = pExtra->Stream();
    }
}

BinaryReader &BinaryReader::operator>>(int16_t &value)
{
    uint8_t vLow, vHigh;
    (*this) >> vLow >> vHigh;
    value = (int16_t)(vLow | ((uint16_t)vHigh << 8));
    return *this;
}

BinaryReader &BinaryReader::operator>>(uint16_t &value)
{
    uint8_t vLow, vHigh;
    (*this) >> vLow >> vHigh;
    value = (uint16_t)(vLow | ((uint16_t)vHigh << 8));
    return *this;
}
BinaryReader &BinaryReader::operator>>(int32_t &value)
{
    uint16_t vLow, vHigh;
    (*this) >> vLow >> vHigh;
    value = (int32_t)(vLow | ((uint32_t)vHigh << 16));
    return *this;
}
BinaryReader &BinaryReader::operator>>(uint32_t &value)
{
    uint16_t vLow, vHigh;
    (*this) >> vLow >> vHigh;
    value = (uint32_t)(vLow | ((uint32_t)vHigh << 16));
    return *this;
}
BinaryReader &BinaryReader::operator>>(int64_t &value)
{
    uint32_t vLow, vHigh;
    (*this) >> vLow >> vHigh;
    value = (int64_t)(vLow | ((uint64_t)vHigh << 32));
    return *this;
}
BinaryReader &BinaryReader::operator>>(uint64_t &value)
{
    uint32_t vLow, vHigh;
    (*this) >> vLow >> vHigh;
    value = (uint64_t)(vLow | ((uint64_t)vHigh << 32));
    return *this;
}

#pragma GCC diagnostic ignored "-Wstrict-aliasing"

BinaryReader &BinaryReader::operator>>(float &value)
{
    uint32_t v;
    (*this) >> v;
    value = *(float *)&v;
    return *this;
}
BinaryReader &BinaryReader::operator>>(double &value)
{
    uint64_t v;
    (*this) >> v;
    value = *(double *)&v;
    return *this;
}
BinaryReader &BinaryReader::operator>>(std::complex<double> &value)
{
    double r, i;
    (*this) >> r >> i;
    value = std::complex<double>(r, i);
    return *this;
}
BinaryReader &BinaryReader::operator>>(std::string &value)
{
    uint32_t length;
    (*this) >> length;
    std::stringstream s;
    for (uint32_t i = 0; i < length; ++i)
    {
        char c;
        (*this) >> c;
        s << c;
    }
    value = s.str();
    return *this;
}
