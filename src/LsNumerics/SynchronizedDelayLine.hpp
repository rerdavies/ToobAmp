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

namespace LsNumerics
{

    constexpr bool TRACE_DELAY_LINE_MESSAGES = false;

    void TraceDelayLineMessage(const std::string &message);

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

    /// @brief Single-writer multiple-reader delay line
    class SynchronizedDelayLine
    {
    public:
        SynchronizedDelayLine() { SetSize(0, 0); }
        SynchronizedDelayLine(
            size_t size,
            size_t audioBufferSize // maximum number of times push can be called before synch is called.
        )
        {
            SetSize(size, audioBufferSize);
        }
        ~SynchronizedDelayLine();

        void SetSize(size_t size, size_t padEntries);

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

    private:
        static constexpr size_t MAX_READ_BORROW = 256;
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
    public:
        static constexpr size_t DEFAULT_LOW_WATER_MARK = std::numeric_limits<size_t>::max();

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

        void SetSize(size_t size, size_t lowWaterMark = DEFAULT_LOW_WATER_MARK)
        {
            if (lowWaterMark == DEFAULT_LOW_WATER_MARK)
            {
                lowWaterMark = size / 2;
            }
            this->lowWaterMark = lowWaterMark;
            buffer.resize(size+ MAX_READ_BORROW);
        }

        void Close()
        {
            std::lock_guard lock{mutex};
            closed = true;
            writeToReadConditionVariable.notify_all();
            readToWriteConditionVariable.notify_all();
        }

    private:
        void ReadWait();

    public:
        float Read()
        {
            if (closed)
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
            std::unique_lock lock{mutex};
            if (closed)
            {
                throw DelayLineClosedException();
            }
            return writeCount + size <= buffer.size();
        }

        void WriteWait()
        {
            std::unique_lock lock{mutex};
            this->readToWriteConditionVariable.wait(lock);
        }

        void Write(size_t count, size_t offset, const std::vector<float> &input)
        {
            if (closed)
            {
                throw DelayLineClosedException();
            }
            while (count != 0)
            {
                size_t thisTime;
                while (true)
                {
                    std::unique_lock lock{mutex};
                    if (closed)
                    {
                        throw DelayLineClosedException();
                    }
                    if (writeCount == buffer.size())
                    {
                        readToWriteConditionVariable.wait(lock);
                    }
                    else
                    {
                        thisTime = buffer.size() - writeCount;
                        break;
                    }
                }
                if (thisTime > count)
                {
                    thisTime = count;
                }
                size_t start = writeHead;
                size_t end = start + thisTime;
                if (end < buffer.size())
                {
                    int writeHead = this->writeHead;
                    for (size_t i = 0; i < thisTime; ++i)
                    {
                        buffer[writeHead++] = input[offset++];
                    }
                    this->writeHead = writeHead;
                    count -= thisTime;
                    this->writeHead = writeHead;
                }
                else
                {
                    size_t writeHead = this->writeHead;
                    size_t count0 = buffer.size() - start;
                    for (size_t i = 0; i < count0; ++i)
                    {
                        buffer[writeHead++] = input[offset++];
                    }
                    size_t count1 = end - buffer.size();
                    writeHead = 0;
                    for (size_t i = 0; i < count1; ++i)
                    {
                        buffer[writeHead++] = input[offset++];
                    }
                    count -= thisTime;
                    this->writeHead = writeHead;
                }
                {
                    std::lock_guard lock{mutex};
                    if (closed)
                    {
                        throw DelayLineClosedException();
                    }
                    this->writeCount += thisTime;
                    writeToReadConditionVariable.notify_all();
                }
            }
        }
        void Write(size_t count, size_t offset, const std::vector<std::complex<double>> &input)
        {
            if (closed)
            {
                throw DelayLineClosedException();
            }
            while (count != 0)
            {
                size_t thisTime;
                while (true)
                {
                    std::unique_lock lock{mutex};
                    if (closed)
                    {
                        throw DelayLineClosedException();
                    }
                    if (writeCount == buffer.size())
                    {
                        readToWriteConditionVariable.wait(lock);
                    }
                    else
                    {
                        thisTime = buffer.size() - writeCount;
                        break;
                    }
                }
                if (thisTime > count)
                {
                    thisTime = count;
                }
                size_t start = writeHead;
                size_t end = start + thisTime;
                if (end < buffer.size())
                {
                    int writeHead = this->writeHead;
                    for (size_t i = 0; i < thisTime; ++i)
                    {
                        buffer[writeHead++] = float(input[offset++].real());
                    }
                    this->writeHead = writeHead;
                    count -= thisTime;
                }
                else
                {
                    size_t writeHead = this->writeHead;
                    size_t count0 = buffer.size() - start;
                    for (size_t i = 0; i < count0; ++i)
                    {
                        buffer[writeHead++] = float(input[offset++].real());
                    }
                    size_t count1 = end - buffer.size();
                    writeHead = 0;
                    for (size_t i = 0; i < count1; ++i)
                    {
                        buffer[writeHead++] = float(input[offset++].real());
                    }
                    count -= thisTime;
                    this->writeHead = writeHead;
                }
                {
                    std::lock_guard lock{mutex};
                    if (closed)
                    {
                        throw DelayLineClosedException();
                    }
                    this->writeCount += thisTime;
                    writeToReadConditionVariable.notify_all();
                }
            }
        }

        size_t GetReadWaits()
        {
            size_t result = readWaits;
            readWaits = 0;
            return result;
        }

    private:
        std::size_t readWaits = 0;
        bool closed = false;
        std::mutex mutex;
        std::size_t writeHead = 0;
        std::size_t readHead = 0;
        std::size_t writeCount = 0;
        std::size_t readCount = 0;
        std::size_t borrowedReads = 0;
        std::size_t lowWaterMark = 0;

        std::condition_variable readToWriteConditionVariable;
        std::condition_variable writeToReadConditionVariable;
        std::vector<float> buffer;
    };
}

#endif // SYNCHRONIZED_DELAY_LINE_HPP