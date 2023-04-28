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

#include "LocklessQueue.hpp"

namespace LsNumerics
{

    enum class SchedulerPolicy { 
        Realtime, // schedule with sufficiently high SCHED_RR priority.
        UnitTest  // set relative priority using nice (3) -- for when the running process may not have sufficient privileges to set a realtime thread priority.
    };

    /// @brief Single-writer multiple-reader delay line
    class AudioThreadToBackgroundQueue
    {
    public:
        SchedulerPolicy schedulerPolicy = SchedulerPolicy::UnitTest;

        AudioThreadToBackgroundQueue() :AudioThreadToBackgroundQueue(0,0,SchedulerPolicy::UnitTest){ }
        AudioThreadToBackgroundQueue(
            size_t size,
            size_t audioBufferSize, // maximum number of times push can be called before synch is called.
            SchedulerPolicy schedulerPolicy

        )
        {
            SetSize(size, audioBufferSize,schedulerPolicy);
        }
        ~AudioThreadToBackgroundQueue();

        void SetSize(size_t size, size_t padEntries, SchedulerPolicy schedulerPolicy);

        void Write(float value)
        {
            storage[head & sizeMask] = value;
            ++head;
        }

        void WriteSynchronized(const float*input, size_t size)
        {
            {
                std::lock_guard lock{mutex};
                for (size_t i = 0; i < size; ++i)
                {
                    Write(input[i]);
                }
                readTail = head;

                if (readTail < (ptrdiff_t)this->size)
                {
                    readHead = 0;  // data is valid from 0 to readTail.
                }
                else
                {
                    readHead = readTail - this->size; // data is valid from readTail-size to readTail.
                }
            }
            readConditionVariable.notify_all();


        }
        void SynchWrite()
        {
            {
                // head is used unsynchronized by the writer.
                // readTail and readHead are communicated under mutex to the reader.
                std::lock_guard lock{mutex};
                readTail = head;
                if (readTail < (ptrdiff_t)size)
                {
                    readHead = 0;  // data is valid from 0 to readTail.
                }
                else
                {
                    readHead = readTail - size; // data is valid from readTail-size to readTail.
                }
            }
            readConditionVariable.notify_all();
        }

        size_t GetReadTailPosition()
        {
            std::lock_guard lock{mutex};
            return readTail;
        }

        size_t WaitForMoreReadData(ptrdiff_t previousTailPosition)
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
        void ReadRange(ptrdiff_t position, size_t size, size_t outputOffset, std::vector<float> &output);
        void ReadRange(ptrdiff_t position, size_t count, std::vector<float> &output)
        {
            ReadRange(position, count, 0, output);
        }

        bool IsReadReady(ptrdiff_t position, size_t count);
        void WaitForRead(ptrdiff_t position, size_t count);

        void Close();

        void CreateThread(const std::function<void(void)> &threadProc, int threadNumber);

        void NotifyReadReady()
        {
            std::lock_guard lock { mutex};
            this->readConditionVariable.notify_all();
        }
        void WaitForStartup()
        {
            std::unique_lock lock(this->mutex);
            while (true)
            {
                if (startedSuccessfully) return;
                if (startupError.length() != 0)
                {
                    throw std::logic_error(startupError);
                }
                this->startConditionVariable.wait(lock);
            }
        }

    private:

        void StartupSucceeded()
        {
            {
                std::lock_guard lock(mutex);
                startedSuccessfully = true;
            }
            startConditionVariable.notify_all();
        }
        void StartupFailed(const std::string &error)
        {
            {
                std::lock_guard lock(mutex);
                startupError = error;
            }
            startConditionVariable.notify_all();
        }

        bool startedSuccessfully = false;
        std::string startupError;

        static constexpr size_t MAX_READ_BORROW = 16;
        bool IsReadReady_(ptrdiff_t position, size_t count);
        bool closed = false;
        std::mutex mutex;
        std::condition_variable readConditionVariable;
        std::condition_variable startConditionVariable;
        std::vector<float> storage;
        std::size_t head = 0;
        std::size_t size = 0;
        std::size_t paddingSize = 0;
        std::size_t sizeMask = 0;
        std::ptrdiff_t readHead = 0;
        std::ptrdiff_t readTail = 0;
        using thread_ptr = std::unique_ptr<std::thread>;

        std::vector<thread_ptr> threads;
    };
}

#endif // SYNCHRONIZED_DELAY_LINE_HPP