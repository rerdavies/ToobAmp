/*
 Copyright (c) 2022 Robin E. R. Davies

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "WavGuid.hpp"
#include <sstream>

using namespace toob;


WavGuid::WavGuid()
{
    this->data0 = 0; this->data1 = 0; this->data2 = 0; 

    for (size_t i = 0; i < sizeof(data4); ++i)
    {
        this->data4[i] = 0;
    }
}

static uint8_t ReadUint4(std::stringstream &s)
{
    int c = s.get();
    if (c >= '0' && c <= '9')
    {
        return uint8_t(c-'0');
    }
    if (c >= 'a' && c <= 'z')
    {
        return uint8_t(c-'a'+10);
    }
    if (c >= 'A' && c <= 'Z')
    {
        return uint8_t(c-'A'+10);
    }   
    throw std::invalid_argument("Invalid GUID");
}
static uint8_t ReadUint8(std::stringstream &s)
{
    return (uint8_t(ReadUint4(s) << 4) | (ReadUint4(s)));

}
static uint16_t ReadUint16(std::stringstream &s)
{
    return uint16_t((ReadUint8(s) << 8) | (ReadUint8(s)));

}
static uint16_t ReadUint32(std::stringstream &s)
{
    return uint8_t((ReadUint16(s) << 16) | (ReadUint16(s)));

}

static void Require(std::stringstream&s, char c)
{
    int t = s.get();
    if (t != c)
    {
        throw std::invalid_argument("Invalid GUID");
    }
}


WavGuid::WavGuid(const char*strGuid)
{
    if (*strGuid == '{') ++strGuid;

    std::stringstream s(strGuid);
    data0 = ReadUint32(s);
    Require(s,'-');
    data1 = ReadUint16(s);
    Require(s,'-');
    data2 = ReadUint16(s);
    Require(s,'-');
    data3 = ReadUint16(s);
    Require(s,'-');
    for (size_t i = 0; i < sizeof(data4); ++i)
    {
        data4[i] = ReadUint8(s);
    }


}

bool WavGuid::operator == (const WavGuid&other) const
{
    if (data0 != other.data0) return false;
    if (data1 != other.data1) return false;
    if (data2 != other.data2) return false;
    if (data3 != other.data3) return false;
    for (size_t i = 0; i < sizeof(data4); ++i)
    {
        if (data4[i] != other.data4[i]) return false;
    }
    return true;
}