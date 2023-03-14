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
#include <sched.h> // posix threads.

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
    this->size = NextPowerOf2(size+paddingSize);
    this->sizeMask = size - 1;
    this->head = 0;
    this->padEntries = size-paddingSize;
    this->storage.resize(0);
    this->storage.resize(size);
    readHead = 0;
    readTail = readHead+size-paddingSize;
}

void SynchronizedDelayLine::ReadLock(size_t position, size_t count)
{
    std::lock_guard guard { mutex};

    if (closed)
    {
        throw ClosedException();
    }

    size_t end = position+count;
    if (position < readHead || end > readTail)
    {
        throw SynchException("Read range not valid.");
    }
}
void SynchronizedDelayLine::ReadUnlock(size_t position, size_t count)
{
    std::lock_guard guard { mutex};
    size_t end = position+count;
    if (closed)
    {
        throw ClosedException();
    }
    if (position < readHead || end > readTail)
    {
        throw SynchException("Read range not valid.");
    }
}


bool SynchronizedDelayLine::IsReadReady(size_t position, size_t count)
{
    std::lock_guard guard { mutex};
    size_t end = position+count;
    if (position >= readHead)
    {
        if (end > readTail)
        {
            throw SynchException("Read underrun.");
        }
        return true;
    }
    return false;
}
void SynchronizedDelayLine::WaitForRead(size_t position, size_t count)
{
    while (true)
    {
        std::lock_guard guard { mutex};

        std::unique_lock<std::mutex> lock { mutex};
        size_t end = position+count;
        if (position >= readHead) 
        {
            if (end > readTail)
            {
                throw SynchException("Read underrun.");
            }
            return;
        }
        this->conditionVariable.wait(lock);
    }

}


void SynchronizedDelayLine::ReadRange(size_t position, size_t size, size_t offset, std::vector<float>&output)
{
    WaitForRead(position,size);
    ReadLock(position,size);

    size_t bufferStart = position & sizeMask;
    size_t bufferEnd = (position+size)&sizeMask;
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
    } else {
        size_t outputIndex = offset;
        for (size_t i = bufferStart; i < bufferEnd; ++i)
        {
            output[outputIndex++] = storage[i];
        }
    }
    ReadUnlock(position,size);
}

void SynchronizedDelayLine::Close()
{
    {
        std::lock_guard guard { mutex};
        closed = true;
    }
    conditionVariable.notify_all();

    for (auto&thread: threads)
    {
        thread->join();
    }
    threads.resize(0);
}

SynchronizedDelayLine::~SynchronizedDelayLine()
{
    Close();
}
void SynchronizedDelayLine::CreateThread(const std::function<void(void)>& threadProc,int relativeThreadPriority)
{

    sched_param schedParam;
    int schedPolicy;

    auto currentThread = pthread_self();
    int ret = pthread_getschedparam(currentThread,&schedPolicy,&schedParam);
    if (ret != 0)
    {
        throw std::logic_error("pthread_getschedparam failed.");
    }

#ifdef WIN32
#error I think priority is inverted for Windows. For XNIX POSIX, decreasing the value increases thread priority.
    schedParam.sched_priority += (sched_priority)relativeThreadPriority; ?
#endif
    schedParam.sched_priority -= relativeThreadPriority;


    thread_ptr thread = std::make_unique<std::thread>(
        [threadProc,schedPolicy,schedParam] () {
            auto currentThread = pthread_self();

            int ret = pthread_setschedparam(currentThread,schedPolicy,&schedParam);
            if (ret != 0) 
            {
                throw std::logic_error("pthread_setschedparam failed.");                
            }
            try {
                threadProc();
            } 
            catch (const ClosedException&)
            {
                // expected and ignored.
            }
            catch (const std::exception&e)
            {
                std::cout << "ERROR: Unexpected exception (" << e.what() << ")" << std::endl;
                std::terminate(); // no hope of recovery.
            }
        }
    );

    Close();
}

