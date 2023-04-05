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
#include "SectionExecutionTrace.hpp"
#include <fstream>

using namespace LsNumerics;
using namespace std;

SectionExecutionTrace::SectionExecutionTrace()
{
    record.reserve(MAX_SIZE);
    startTime = clock::now();
}

SectionExecutionTrace::~SectionExecutionTrace()
{
    WriteRecord();
}

double SectionExecutionTrace::ToDisplayTime(const time_point& time)
{
    clock::duration t = time-startTime;

    return std::chrono::duration_cast<std::chrono::seconds>(t).count()*1000;

}


void SectionExecutionTrace::WriteRecord(const std::filesystem::path fileName)
{
    if (dumped) return;
    dumped = true;

    if (record.size() == 0) return;

    std::fstream f(fileName);

    // header row for Excel.
    f << "threadNumber"
      << ","
      << "size"
      << ","
      << "start"
      << ","
      << "end"
      << ","
      << "writeCount"
      << ","
      << "inputOffset"
      << endl;

    for (auto &entry : record)
    {
        f << entry.threadNumber
          << "," << entry.size
          << "," << ToDisplayTime(entry.start)
          << "," << ToDisplayTime(entry.end)
          << "," << entry.writeCount
          << "," << entry.inputOffset
          << endl;
    }
}