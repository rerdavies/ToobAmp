/*
Copyright (c) 2023 Robin E. R. Davies

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

#include "SvgPathWriter.hpp"


using namespace toob;




void SvgPathWriter::SetPrecision(int digits) { ss.precision(digits); }
void SvgPathWriter::MoveTo(double x, double y) {
    ss << "M" <<x << ',' << y;
    lastX = x; lastY = y;
}
void SvgPathWriter::LineTo(double x, double y)
{
    if (x == lastX)
    {
        ss << "V" << y;
    } else if (y == lastY)
    {
        ss << "H" << x;
    } else {
        ss << "L" << x << ',' << y;
    }
    lastX = x;
    lastY = y;
}
void SvgPathWriter::Close() {
    ss << "Z";
}


std::string SvgPathWriter::String()
{
    return ss.str();
}
