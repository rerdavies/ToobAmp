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

#include <chrono>
#include <vector>
#include <mutex>
#include <filesystem>

namespace LsNumerics
{
// debug utility for tracing execution, Make sure it NEVER leaks into production code.
#if defined(RELWITHDEBINFO)
#define EXECUTION_TRACE 1
#else
#define EXECUTION_TRACE 0
#endif

#if EXECUTION_TRACE
    /// @brief A tool for logging execution schedules of ConvolutionReverb sections.
    class SectionExecutionTrace
    {
    public:
        static constexpr size_t MAX_SIZE = 500;
        using clock = std::chrono::high_resolution_clock;
        using time_point = clock::time_point;

        SectionExecutionTrace();
        ~SectionExecutionTrace();

        void Trace(size_t threadNumber, size_t size, time_point start, time_point end, size_t writeCount, size_t inputOffset);
        void WriteRecord(const std::filesystem::path fileName = std::filesystem::path("/tmp/sectionTrace.txt"));

    private:
        double ToDisplayTime(const time_point& t);
        std::mutex mutex;
        time_point startTime;
        struct TraceEntry
        {
            size_t threadNumber;
            size_t size;
            time_point start;
            time_point end;
            size_t writeCount;
            size_t inputOffset;
        };
        bool dumped;
        std::vector<TraceEntry> record;
    };

    inline void SectionExecutionTrace::Trace(size_t threadNumber, size_t size, time_point start, time_point end, size_t writeCount, size_t inputOffset)
    {
        std::lock_guard<std::mutex> lock{mutex};
        if (record.size() < MAX_SIZE)
        {
            record.push_back(TraceEntry{threadNumber, size, start, end, inputOffset});
        }
    }

#endif
}