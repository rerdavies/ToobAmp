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

#ifndef SYNCHRONIZED_DELAY_LINE_HPP
#define SYNCHRONIZED_DELAY_LINE_HPP

#include <cstddef>
#include <vector>
#include <condition_variable>
#include <stdexcept>
#include <thread>
#include <functional>
#include <limits>
#include <complex>
#include <chrono>
#include <atomic>
#include "SectionExecutionTrace.hpp"

namespace LsNumerics
{
    constexpr bool TRACE_BACKGROUND_CONVOLUTION_MESSAGES = false;

    void TraceBackgroundConvolutionMessage(const std::string &message);

    class DelayLineClosedException : public std::logic_error
    {
    public:
        DelayLineClosedException()
            : std::logic_error("Closed.")
        {
        }
    };
    class DelayLineSynchException : public std::logic_error
    {
    public:
        DelayLineSynchException(const std::string &message)
            : std::logic_error(message)
        {
        }
    };


    enum class SchedulerPolicy { 
        Realtime, // schedule with sufficiently high SCHED_RR priority.
        UnitTest  // set relative priority using nice (3) -- for when the running process may not have sufficient privileges to set a realtime thread priority.
    };

    /// @brief Single-writer multiple-reader delay line
    class BackgroundConvolutionTask
    {
    public:
        SchedulerPolicy schedulerPolicy = SchedulerPolicy::UnitTest;

        BackgroundConvolutionTask() :BackgroundConvolutionTask(0,0,SchedulerPolicy::UnitTest){ }
        BackgroundConvolutionTask(
            size_t size,
            size_t audioBufferSize, // maximum number of times push can be called before synch is called.
            SchedulerPolicy schedulerPolicy

        )
        {
            SetSize(size, audioBufferSize,schedulerPolicy);
        }
        ~BackgroundConvolutionTask();

        void SetSize(size_t size, size_t padEntries, SchedulerPolicy schedulerPolicy);

        void Write(float value)
        {
            storage[head & sizeMask] = value;
            ++head;
        }

        void SynchWrite()
        {
            std::lock_guard lock{mutex};
            readTail = head;
            if (readTail < size)
            {
                readHead = 0;
            }
            else
            {
                readHead = readTail - size;
            }
            readConditionVariable.notify_all();
        }

        size_t GetReadTailPosition()
        {
            std::lock_guard lock{mutex};
            return readTail;
        }

        size_t WaitForMoreReadData(size_t previousTailPosition)
        {
            while (true)
            {
                std::unique_lock lock{mutex};
                if (closed)
                {
                    throw DelayLineClosedException();
                }
                if (readTail != previousTailPosition)
                {
                    return readTail;
                }
                this->readConditionVariable.wait(lock);
            }
        }

        float At(size_t index) const
        {
            return storage[(head - 1 - index) & sizeMask];
        }

        float operator[](size_t index) const
        {
            return At(index);
        }

        void ReadLock(size_t position, size_t size);
        void ReadUnlock(size_t poisiont, size_t size);
        void ReadWait()
        {
            std::unique_lock lock{mutex};
            if (closed) {
                throw DelayLineClosedException();
            }
            readConditionVariable.wait(lock);
        }
        void ReadRange(size_t position, size_t size, size_t outputOffset, std::vector<float> &output);
        void ReadRange(size_t position, size_t count, std::vector<float> &output)
        {
            ReadRange(position, count, 0, output);
        }

        bool IsReadReady(size_t position, size_t count);
        void WaitForRead(size_t position, size_t count);

        void Close();

        void CreateThread(const std::function<void(void)> &threadProc, int relativeThreadPriority);

        void NotifyReadReady()
        {
            std::lock_guard lock { mutex};
            this->readConditionVariable.notify_all();
        }


    private:
        static constexpr size_t MAX_READ_BORROW = 16;
        bool IsReadReady_(size_t position, size_t count);
        bool closed = false;
        std::mutex mutex;
        std::condition_variable readConditionVariable;
        std::vector<float> storage;
        std::size_t head = 0;
        std::size_t size = 0;
        std::size_t paddingSize = 0;
        std::size_t sizeMask = 0;
        std::size_t readHead = 0;
        std::size_t readTail = 0;
        using thread_ptr = std::unique_ptr<std::thread>;

        std::vector<thread_ptr> threads;
    };

    class SynchronizedSingleReaderDelayLine
    {
    private:

        static constexpr size_t MAX_READ_BORROW = 16;
        static constexpr std::chrono::milliseconds READ_TIMEOUT{10000};
    public:
        static constexpr size_t DEFAULT_LOW_WATER_MARK = std::numeric_limits<size_t>::max();

        class IDelayLineCallback {
        public:
            virtual void OnSynchronizedSingleReaderDelayLineReady() = 0;
            virtual void OnSynchronizedSingleReaderDelayLineUnderrun() = 0;
        };

        SynchronizedSingleReaderDelayLine(size_t size, size_t lowWaterMark = DEFAULT_LOW_WATER_MARK)
        {
            SetSize(size, lowWaterMark);
        }
        SynchronizedSingleReaderDelayLine()
            : SynchronizedSingleReaderDelayLine(0, 0) 
        {
        }
        ~SynchronizedSingleReaderDelayLine()
        {
            Close();
        }

        uint32_t GetWriteCount()
        {
            return this->atomicWriteCount.load();
        }
        void SetWriteReadyCallback(IDelayLineCallback *callback)
        {
            this->writeReadyCallback = callback;
        }

        void SetSize(size_t size, size_t lowWaterMark = DEFAULT_LOW_WATER_MARK)
        {
            if (lowWaterMark == DEFAULT_LOW_WATER_MARK)
            {
                lowWaterMark = size / 2;
            }
            this->lowWaterMark = lowWaterMark+MAX_READ_BORROW;
            if (size != 0)
            {
                buffer.resize(size+ MAX_READ_BORROW);
            }
        }

        void Close()
        {
            std::lock_guard lock{mutex};
            atomicClosed = true;
            writeStalled = false;
            writeToReadConditionVariable.notify_all();
            readToWriteConditionVariable.notify_all();
        }

    private:
        IDelayLineCallback *writeReadyCallback = nullptr;
        void ReadWait();

    public:
        float Read()
        {
            if (atomicClosed)
            {
                throw DelayLineClosedException();
            }
            if (readCount == 0)
            {
                ReadWait();
            }
            --readCount;
            float result = buffer[readHead++];
            if (readHead == buffer.size())
            {
                readHead = 0;
            }
            return result;
        }


        bool CanWrite(size_t size)
        {
            if (atomicClosed.load())
            {
                throw DelayLineClosedException();
            }
            if (wWriteCount + size <= buffer.size()) return true;
            wWriteCount = atomicWriteCount.load();
            bool result =  wWriteCount + size <= buffer.size();
            if (!result)
            {
                writeStalled.store(true);
            }
            return result;
        }

        void WriteWait()
        {
            std::unique_lock lock{mutex};
            writeStalled = true;
            this->readToWriteConditionVariable.wait(lock);
        }

        void Write(size_t count, size_t offset, const std::vector<float> &input);

        void Write(size_t count, size_t offset, const std::vector<std::complex<double>> &input);

        size_t GetReadWaits()
        {
            size_t result = readWaits;
            readWaits = 0;
            return result;
        }

    private:
        std::atomic<bool> writeStalled = false;
        std::atomic<uint32_t> atomicWriteCount = 0;
        uint32_t rWriteCount = 0;
        uint32_t wWriteCount = 0;
        std::atomic<bool> atomicClosed = false;

        std::size_t readWaits = 0;
        std::mutex mutex;
        std::uint32_t writeHead = 0;
        std::uint32_t readHead = 0;
        std::uint32_t readCount = 0;
        std::uint32_t borrowedReads = 0;
        std::uint32_t lowWaterMark = 0;

        std::condition_variable readToWriteConditionVariable;
        std::condition_variable writeToReadConditionVariable;
        std::vector<float> buffer;
    };
}

#endif // SYNCHRONIZED_DELAY_LINE_HPP