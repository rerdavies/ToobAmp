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

#include "AudioThreadToBackgroundQueue.hpp"
#include <iostream>
#include <exception>
#include <pthread.h> // for changing thread priorit.
#include <sched.h>   // posix threads.
#include <unistd.h>  // for nice()
#include <iostream>
#include <cstring> // for memset.
#include "../util.hpp"
#include "../ss.hpp"

using namespace LsNumerics;

// #define WRITE_BARRIER() __dmb(14)
// #define READ_BARRIER() __dmb(15)

#define READ_BARRIER() std::atomic_thread_fence(std::memory_order_acquire)
#define WRITE_BARRIER() std::atomic_thread_fence(std::memory_order_release); // Ensure that data in the buffer is flushed.

// #define READ_BARRIER() ((void)0)
// #define WRITE_BARRIER() ((void)0)

// std::atomic_thread_fence(std::memory_order_release); // flush buffer data.

static int NextPowerOf2(size_t value)
{
    size_t result = 1;
    while (result < value)
    {
        result *= 2;
    }
    return result;
}

void AudioThreadToBackgroundQueue::SetSize(size_t size, size_t padEntries, SchedulerPolicy schedulerPolicy)
{
    this->schedulerPolicy = schedulerPolicy;
    size = NextPowerOf2(size + padEntries + 1024);
    this->size = size;
    this->sizeMask = size - 1;
    this->head = 0;
    this->paddingSize = paddingSize;
    this->storage.resize(0);
    this->storage.resize(size);
    readHead = 0;
    readTail = 0;
}

inline bool AudioThreadToBackgroundQueue::IsReadReady_(ptrdiff_t position, size_t size)
{
    if (closed)
    {
        throw DelayLineClosedException();
    }

    ptrdiff_t end = position + (ptrdiff_t)size;
    if (position < readHead && position >= 0)
    {
        throw DelayLineSynchException("AudioThreadToBackgroundQueue underrun.");
    }
    if (end <= readTail)
    {
        return true;
    }
    return false;
}
bool AudioThreadToBackgroundQueue::IsReadReady(ptrdiff_t position, size_t size)
{
    std::lock_guard guard{mutex};

    return IsReadReady_(position, size);
}

void AudioThreadToBackgroundQueue::ReadLock(size_t position, size_t count)
{
    std::lock_guard guard{mutex};
    if (!IsReadReady_(position, count))
    {
        throw DelayLineSynchException("Read range not valid.");
    }
}
void AudioThreadToBackgroundQueue::ReadUnlock(size_t position, size_t count)
{
    std::lock_guard guard{mutex};
    if (!IsReadReady_(position, count))
    {
        throw DelayLineSynchException("Read range not valid.");
    }
}

void AudioThreadToBackgroundQueue::WaitForRead(ptrdiff_t position, size_t count)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock{mutex};
        if (IsReadReady_(position, count))
        {
            return;
        }

        this->readConditionVariable.wait(lock);
    }
}

void AudioThreadToBackgroundQueue::ReadRange(ptrdiff_t position, size_t size, size_t offset, std::vector<float> &output)
{
    WaitForRead(position, size);

    size_t bufferStart = position & sizeMask;
    size_t bufferEnd = (position + size) & sizeMask;
    if (bufferEnd < bufferStart)
    {
        size_t outputIndex = offset;
        for (size_t i = bufferStart; i < this->storage.size(); ++i)
        {
            output[outputIndex++] = storage[i];
        }
        for (size_t i = 0; i < bufferEnd; ++i)
        {
            output[outputIndex++] = storage[i];
        }
    }
    else
    {
        size_t outputIndex = offset;
        for (size_t i = bufferStart; i < bufferEnd; ++i)
        {
            output[outputIndex++] = storage[i];
        }
    }
    ReadUnlock(position, size);
}

void AudioThreadToBackgroundQueue::Close()
{
    {
        std::lock_guard guard{mutex};
        closed = true;
        readConditionVariable.notify_all();
    }

    for (auto &thread : threads)
    {
        thread->join();
    }
    threads.resize(0);
}

AudioThreadToBackgroundQueue::~AudioThreadToBackgroundQueue()
{
    try
    {
        Close();
    }
    catch (const std::exception &e)
    {
        std::cout << "FATAL ERROR: Unexpected error while closing AudioThreadToBackgroundQueue. (" << e.what() << ")" << std::endl;
        std::terminate();
    }
}

static int convolutionThreadPriorities[] =
    {
        -1,
        45,
        44,
        4,
        3,
        2,
        1,
        1,
        1,
        1,
        1,
        1,
};

void AudioThreadToBackgroundQueue::CreateThread(const std::function<void(void)> &threadProc, int threadNumber)
{
    if ((size_t)threadNumber > sizeof(convolutionThreadPriorities) / sizeof(convolutionThreadPriorities[0]) || threadNumber == 0)
    {
        throw std::logic_error("Invalid thread number.");
    }
    this->startedSuccessfully = false;
    this->startupError = "";

    thread_ptr thread = std::make_unique<std::thread>(
        [this, threadProc, threadNumber]()
        {
            toob::SetThreadName(SS("crvb" << threadNumber));

            if (this->schedulerPolicy == SchedulerPolicy::UnitTest)
            {
                errno = 0;
                int ret = nice(threadNumber);
                if (ret < 0 && errno != 0)
                {
                    this->StartupFailed(
                        "Can't reduce priority of BalancedConvolution thread.");
                    return;
                }
                this->StartupSucceeded();
            }
            else
            {
                try
                {
                    int schedPriority = convolutionThreadPriorities[threadNumber];
                    toob::SetRtThreadPriority(schedPriority);
                }
                catch (const std::exception &e)
                {
                    this->StartupFailed(
                        SS("Unable to set realtime thread priority. See https://rerdavies.github.io/pipedal/RTThreadPriority.html for further instructions. "
                           << "(" << e.what() << ")"));
                    return;
                }
                this->StartupSucceeded();
            }
            try
            {
                threadProc();
            }
            catch (const DelayLineClosedException &)
            {
                // expected and ignored.
            }
            catch (const std::exception &e)
            {
                std::cout << "ERROR: Unexpected exception in SynchronizedConvolution service thread. (" << e.what() << ")" << std::endl;
                throw; // will terminate.
            }
        });
    this->threads.push_back(std::move(thread));
    this->WaitForStartup();
}
