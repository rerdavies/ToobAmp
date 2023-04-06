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

#include "BackgroundConvolutionTask.hpp"
#include <iostream>
#include <exception>
#include <pthread.h> // for changing thread priorit.
#include <sched.h>   // posix threads.
#include <unistd.h>  // for nice()
#include <iostream>
#include <cstring> // for memset.
#include "../util.hpp"
#include "../ss.hpp"
//#include <semaphore>


using namespace LsNumerics;

//#define WRITE_BARRIER() __dmb(14)
//#define READ_BARRIER() __dmb(15)


// #define READ_BARRIER() std::atomic_thread_fence(std::memory_order_acquire)
// #define WRITE_BARRIER() std::atomic_thread_fence(std::memory_order_release); // Ensure that data in the buffer is flushed.


#define READ_BARRIER() ((void)0)
#define WRITE_BARRIER() ((void)0)



//std::atomic_thread_fence(std::memory_order_release); // flush buffer data.

static int NextPowerOf2(size_t value)
{
    size_t result = 1;
    while (result < value)
    {
        result *= 2;
    }
    return result;
}

void BackgroundConvolutionTask::SetSize(size_t size, size_t padEntries, SchedulerPolicy schedulerPolicy)
{
    this->schedulerPolicy = schedulerPolicy;
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

inline bool BackgroundConvolutionTask::IsReadReady_(size_t position, size_t size)
{
    if (closed)
    {
        throw DelayLineClosedException();
    }

    size_t end = position + size;
    if (position < readHead)
    {
        throw DelayLineSynchException("BackgroundConvolutionTask underrun.");
    }
    if (end <= readTail)
    {
        return true;
    }
    return false;
}
bool BackgroundConvolutionTask::IsReadReady(size_t position, size_t size)
{
    std::lock_guard guard{mutex};

    return IsReadReady_(position, size);
}

void BackgroundConvolutionTask::ReadLock(size_t position, size_t count)
{
    std::lock_guard guard{mutex};
    if (!IsReadReady_(position, count))
    {
        throw DelayLineSynchException("Read range not valid.");
    }
}
void BackgroundConvolutionTask::ReadUnlock(size_t position, size_t count)
{
    std::lock_guard guard{mutex};
    if (!IsReadReady_(position, count))
    {
        throw DelayLineSynchException("Read range not valid.");
    }
}

void BackgroundConvolutionTask::WaitForRead(size_t position, size_t count)
{
    while (true)
    {

        std::unique_lock<std::mutex> lock{mutex};
        if (IsReadReady_(position, count))
        {
            return;
        }
        if (TRACE_BACKGROUND_CONVOLUTION_MESSAGES)
        {
            TraceBackgroundConvolutionMessage("BackgroundConvolutionTask: wait for read.");
        }

        this->readConditionVariable.wait(lock);
    }
}

void BackgroundConvolutionTask::ReadRange(size_t position, size_t size, size_t offset, std::vector<float> &output)
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

void BackgroundConvolutionTask::Close()
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

BackgroundConvolutionTask::~BackgroundConvolutionTask()
{
    try
    {
        Close();
    }
    catch (const std::exception &e)
    {
        std::cout << "FATAL ERROR: Unexpected error while closing BackgroundConvolutionTask. (" << e.what() << ")" << std::endl;
        std::terminate();
    }
}
void BackgroundConvolutionTask::CreateThread(const std::function<void(void)> &threadProc, int relativeThreadPriority)
{

    thread_ptr thread = std::make_unique<std::thread>(
        [this, threadProc, relativeThreadPriority]()
        {
            toob::SetThreadName(SS("rvb" << -relativeThreadPriority));

            if (this->schedulerPolicy == SchedulerPolicy::UnitTest)
            {
                errno = 0;
                int ret = nice(1 - relativeThreadPriority / 3);
                if (ret < 0 && errno != 0)
                {
                    throw std::logic_error("Can't reduce priority of BalancedConvolution thread.");
                }
            }
            else
            {
                nice(0);
                int schedPolicy = SCHED_RR;
                int priorityMin = sched_get_priority_min(schedPolicy);
                int priorityMax = sched_get_priority_max(schedPolicy);
                // constexpr int USB_SERVICE_THREAD_PRIORITY = 5;
                constexpr int BASE_THREAD_PRIORITY = 25;
                int schedPriority = BASE_THREAD_PRIORITY - 1 + relativeThreadPriority /2;

                if (schedPriority < priorityMin)
                {
                    schedPriority = priorityMin;
                }
                if (schedPriority >= priorityMax)
                {
                    throw std::logic_error(SS("BalancedConvolution thread priority above maximum value. (" << priorityMax << ")"));
                }

                sched_param schedParam;
                memset(&schedParam, 0, sizeof(schedParam));
                schedParam.sched_priority = schedPriority;

                int ret = sched_setscheduler(0, schedPolicy, &schedParam);
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
        if (borrowedReads != 0)
        {
            auto previousValue = atomicWriteCount.fetch_sub(borrowedReads);
            rWriteCount -= borrowedReads; // don't update rWriteCount. rWriteCount controls when we have to do ultra-expensive READ_BARRIER() calls.
            auto currentValue = previousValue - borrowedReads;
            borrowedReads = 0;
            if (previousValue > this->lowWaterMark && currentValue <= this->lowWaterMark)
            {
                bool writeStalled = this->writeStalled.exchange(false);
                if (writeStalled)
                {
                    this->readToWriteConditionVariable.notify_all();
                }
            }
        }
        if (rWriteCount  < MAX_READ_BORROW)
        {
            rWriteCount = atomicWriteCount.load();
            if (rWriteCount != 0)
            {
                READ_BARRIER(); // read barrier for buffer memory.
            }
        }
        size_t available = rWriteCount;

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

        // Everything after this point should never happen on a realtime audio thread.
        // Either (1), we're running a unit test, and the test thread is *pulling* data,
        // or (2) The audio thread has underrun.
        // If an underrun, the right thing to do is wait (and cause the audio thread to
        // underrun, because if we drop, sync is permanently lost.

        ++readWaits; // should never happen in our application.

        if (TRACE_BACKGROUND_CONVOLUTION_MESSAGES)
        {
            TraceBackgroundConvolutionMessage("BackgroundConvolutionTask: wait for read.");
        }
        std::cout << "BackgroundConvolutionTask read underrun." << std::endl;
        writeReadyCallback->OnSynchronizedSingleReaderDelayLineUnderrun();
        {
            readToWriteConditionVariable.notify_all();
            writeReadyCallback->OnSynchronizedSingleReaderDelayLineReady();
            {
                std::unique_lock lock{mutex};

                if (atomicWriteCount.load() == 0) // TODO: WHY DOES THIS WORK? (We deadlock without it)
                {
                    if (writeToReadConditionVariable.wait_for(lock, READ_TIMEOUT) == std::cv_status::timeout)
                    {
                        throw DelayLineSynchException("Read stalled.");
                    }
                }
            }
        }
    }
}

static std::mutex messageMutex;
void LsNumerics::TraceBackgroundConvolutionMessage(const std::string &message)
{
    std::lock_guard lock{messageMutex};
    std::cout << message << std::endl;
}

void SynchronizedSingleReaderDelayLine::Write(size_t count, size_t offset, const std::vector<std::complex<double>> &input)
{
    while (count != 0)
    {
        size_t thisTime;
        while (true)
        {
            if (atomicClosed)
            {
                throw DelayLineClosedException();
            }
            if (wWriteCount+ count >= buffer.size())
            {
                wWriteCount = atomicWriteCount.load();
            }
            if (wWriteCount == buffer.size())
            {
                writeStalled = true;
                {
                    std::unique_lock<std::mutex> lock{mutex};
                    readToWriteConditionVariable.wait(lock);
                }
            }
            else
            {
                thisTime = buffer.size() - wWriteCount;

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
            
            if (atomicClosed.load())
            {
                throw DelayLineClosedException();
            }
            WRITE_BARRIER();
            this->atomicWriteCount += thisTime; // and release the reader with an atomic operation.
            wWriteCount += thisTime;

            writeToReadConditionVariable.notify_all();
        }
    }
}
void SynchronizedSingleReaderDelayLine::Write(size_t count, size_t offset, const std::vector<float> &input)
{
    while (count != 0)
    {
        size_t thisTime;
        while (true)
        {
            std::unique_lock lock{mutex};
            if (atomicClosed.load())
            {
                throw DelayLineClosedException();
            }
            if (wWriteCount+count < buffer.size())
            {
                thisTime = count;
                break;
            }
            wWriteCount = this->atomicWriteCount.load();
            if (wWriteCount == buffer.size())
            {
                writeStalled = true;
                readToWriteConditionVariable.wait(lock);
            }
            else
            {
                thisTime = buffer.size() - wWriteCount;
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

        WRITE_BARRIER(); 
        this->atomicWriteCount += thisTime; // and release the reader (atomic operation)
        this->wWriteCount += thisTime;

        writeToReadConditionVariable.notify_all();
    }
}
