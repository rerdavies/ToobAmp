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

#include "Ce2Chorus.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace TwoPlay;
using namespace std;

int main(int argc, char**argv)
{
    Ce2Chorus chorus;
    double sampleRate = 48000;
    chorus.SetSampleRate(sampleRate);
    Ce2Chorus::Instrumentation chorusTest(&chorus);

    chorus.SetDepth(0.5);
    chorus.SetRate(1);

    std::filesystem::path path = std::filesystem::path(getenv("HOME")) / "chorusTest.tsv";

    std::ofstream f;
    f.open(path);

    for (int i = 0; i < 500; ++i)
    {
        for (int j = 0; j < 100-1; ++j)
        {
            chorusTest.TickLfo();
        }
        f << chorusTest.TickLfo() << endl;
    }

    // /* Data around the first maximum */
    // for (int i = 0; i < 3*sampleRate/3.25/4-50; ++i)
    // {
    //     chorusTest.TickLfo();
    // }
    // for (int i = 0; i < 600; ++i)
    // {
    //     f << chorusTest.TickLfo() << endl;
    // }


    return 0;
}