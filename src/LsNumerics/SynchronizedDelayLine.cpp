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

#include "SynchronizedDelayLine.hpp"
#include <iostream>
#include <exception>
#include <pthread.h> // for changing thread priorit.
#include <sched.h>   // posix threads.
#include <unistd.h> // for nice()
#include <iostream>

using namespace LsNumerics;

static int NextPowerOf2(size_t value)
{
    size_t result = 1;
    while (result < value)
    {
        result *= 2;
    }
    return result;
}

void SynchronizedDelayLine::SetSize(size_t size, size_t paddingSize)
{
    size = NextPowerOf2(size);
    this->size = size;
    this->sizeMask = size - 1;
    this->head = 0;
    this->paddingSize = paddingSize;
    this->storage.resize(0);
    this->storage.resize(size);
    readHead = 0;
    readTail = 0;
}


inline bool SynchronizedDelayLine::IsReadReady_(size_t position, size_t size)
{
    if (closed)
    {
        throw DelayLineClosedException();
    }

    size_t end = position + size;
    if (position < readHead)
    {
        throw DelayLineSynchException("SynchronizedDelayLine underrun.");
    }
    if (end <= readTail) 
    {
        return true;
    }
    return false;

}
bool SynchronizedDelayLine::IsReadReady(size_t position, size_t size)
{
    std::lock_guard guard{mutex};

    return IsReadReady_(position,size);
}
        
void SynchronizedDelayLine::ReadLock(size_t position, size_t count)
{
    std::lock_guard guard{mutex};
    if (!IsReadReady_(position,count))
    {
        throw DelayLineSynchException("Read range not valid.");
    }

}
void SynchronizedDelayLine::ReadUnlock(size_t position, size_t count)
{
    std::lock_guard guard{mutex};
    if (!IsReadReady_(position,count))
    {
        throw DelayLineSynchException("Read range not valid.");
    }
}

void SynchronizedDelayLine::WaitForRead(size_t position, size_t count)
{
    while (true)
    {

        std::unique_lock<std::mutex> lock{mutex};
        if (IsReadReady_(position,count))
        {
            return;
        }
        if (TRACE_DELAY_LINE_MESSAGES)
        {
            TraceDelayLineMessage("SynchronizedDelayLine: wait for read.");
        }

        this->readConditionVariable.wait(lock);
    }
}

void SynchronizedDelayLine::ReadRange(size_t position, size_t size, size_t offset, std::vector<float> &output)
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

void SynchronizedDelayLine::Close()
{
    {
        std::lock_guard guard{mutex};
        closed = true;
    }
    readConditionVariable.notify_all();

    for (auto &thread : threads)
    {
        thread->join();
    }
    threads.resize(0);
}

SynchronizedDelayLine::~SynchronizedDelayLine()
{
    try {
        Close();
    } catch (const std::exception&e)
    {
        std::cout << "FATAL ERROR: Unexpected error while closing SynchronizedDelayLine. (" << e.what() << ")" << std::endl;
        std::terminate();
    }
}
void SynchronizedDelayLine::CreateThread(const std::function<void(void)> &threadProc, int relativeThreadPriority)
{

    sched_param schedParam;
    int schedPolicy;

    auto currentThread = pthread_self();
    int ret = pthread_getschedparam(currentThread, &schedPolicy, &schedParam);
    if (ret != 0)
    {
        throw std::logic_error("pthread_getschedparam failed.");
    }

#ifdef WIN32
#error I think priority is inverted for Windows. For XNIX POSIX, decreasing the value increases thread priority.
    // please let the author know which way this goes so the comment can be removed.
    schedParam.sched_priority += (sched_priority)relativeThreadPriority;
    ?
#endif
    schedParam.sched_priority += relativeThreadPriority;

    thread_ptr thread = std::make_unique<std::thread>(
        [threadProc, relativeThreadPriority,schedPolicy, schedParam]()
        {
            auto currentThread = pthread_self();

            if (schedPolicy == SCHED_OTHER)
            {
                nice(-relativeThreadPriority);
            } else {
                int priorityMin = sched_get_priority_min(schedPolicy);
                if (schedParam.sched_priority < priorityMin)
                {
                    throw std::logic_error("BalancedConvolution thread priority below minimum value.");
                }
                int ret = pthread_setschedparam(currentThread, schedPolicy, &schedParam);
                if (ret != 0)
                {
                    throw std::logic_error("pthread_setschedparam failed.");
                }
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

}

void SynchronizedSingleReaderDelayLine::ReadWait()
{
    while (readCount == 0)
    {
        std::unique_lock lock{mutex};

        if (closed)
        {
            throw DelayLineClosedException();
        }
        writeCount -= borrowedReads;
        if (borrowedReads != 0)
        {
            readToWriteConditionVariable.notify_all();
            borrowedReads = 0;
        }
        size_t available = writeCount;

        // only synchronize every N samples for efficiency's sake.
        // The reader temporarily "borrows" n bytes from the buffer.

        if (available > MAX_READ_BORROW) 
        {
            available = MAX_READ_BORROW;
        }
        if (available > 0)
        {
            borrowedReads = available;
            readCount = available;
            break;
        }
        ++readWaits; // should never happen in our application.

        if (TRACE_DELAY_LINE_MESSAGES)
        {
            TraceDelayLineMessage("SynchronizedDelayLine: wait for read.");
        }
        writeToReadConditionVariable.wait(lock);
    }
}

static std::mutex messageMutex;
void LsNumerics::TraceDelayLineMessage(const std::string & message)
{
    std::lock_guard lock { messageMutex};
    std::cout << message << std::endl;
}