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

#include "LocklessQueue.hpp"
#include <iostream>

//#define WRITE_BARRIER() __dmb(14)
//#define READ_BARRIER() __dmb(15)


#define READ_BARRIER() std::atomic_thread_fence(std::memory_order_acquire)
#define WRITE_BARRIER() std::atomic_thread_fence(std::memory_order_release); // Ensure that data in the buffer is flushed.


// #define READ_BARRIER() ((void)0)
// #define WRITE_BARRIER() ((void)0)


using namespace LsNumerics;

void LocklessQueue::ReadWait()
{
    while (readCount == 0)
    {
        if (this->atomicClosed)
        {
            throw std::runtime_error("Closed");
        }
        if (borrowedReads != 0)
        {
            auto previousValue = atomicWriteCount.fetch_sub(borrowedReads);
            rWriteCount -= borrowedReads; // don't update rWriteCount. rWriteCount controls when we have to do ultra-expensive READ_BARRIER() calls.
            auto currentValue = previousValue - borrowedReads;
            borrowedReads = 0;
            if (previousValue > this->lowWaterMark && currentValue <= this->lowWaterMark)
            {
                this->writeStalled.exchange(false);
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
        // If an underrun, the right thing to do is spin-wait, causing the  audio thread to
        // underrun. If we drop, sync is permanently lost.

        ++readWaits; // should never happen in our application.

        writeReadyCallback->OnSynchronizedSingleReaderDelayLineUnderrun();
        {
            writeReadyCallback->OnSynchronizedSingleReaderDelayLineReady();
            {
                using clock = std::chrono::steady_clock;
                clock::time_point start = clock::now();
                clock::time_point end = start + std::chrono::duration_cast<clock::duration>(std::chrono::milliseconds(500));

                while (true)
                {
                    if (atomicClosed)
                    {
                        throw DelayLineClosedException();
                    }
                    bool done = false;
                    // spin for a bit.
                    for (size_t i = 0; i < 10000; ++i)
                    {
                        auto writeCount = atomicWriteCount.load();
                        if (writeCount != 0)
                        {
                            done = true;
                            break;
                        }
                    }
                    if (done) break;
                    // check to see if timeout has expired.
                    if (clock::now() > end)
                    {
//                        throw DelayLineSynchException("Read stalled.");
                    }
                }
            }
        }
    }
}



void LocklessQueue::Write(size_t count, size_t offset, const std::vector<std::complex<double>> &input)
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
                    // need to check for space before calling write.
                    throw DelayLineSynchException("Write sync lost.");
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

        }
    }
}
void LocklessQueue::Write(size_t count, size_t offset, const std::vector<std::complex<double>> &inputLeft, const std::vector<std::complex<double>> &inputRight)
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
                    // need to check for space before calling write.
                    throw DelayLineSynchException("Write sync lost.");
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
                buffer[writeHead] = float(inputLeft[offset].real());
                bufferRight[writeHead++] = float(inputRight[offset++].real());
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
                buffer[writeHead] = float(inputLeft[offset].real());
                bufferRight[writeHead++] = float(inputRight[offset++].real());
            }
            size_t count1 = end - buffer.size();
            writeHead = 0;
            for (size_t i = 0; i < count1; ++i)
            {
                buffer[writeHead] = float(inputLeft[offset].real());
                bufferRight[writeHead++] = float(inputRight[offset++].real());
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

        }
    }
}
void LocklessQueue::Write(size_t count, size_t offset, const std::vector<float> &input)
{
    while (count != 0)
    {
        size_t thisTime;
        while (true)
        {
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
                throw DelayLineSynchException("Write sync lost.");
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
    }
}

void LocklessQueue::Write(size_t count, size_t offset, const std::vector<float> &inputLeft, const std::vector<float> &inputRight)
{
    while (count != 0)
    {
        size_t thisTime;
        while (true)
        {
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
                throw DelayLineSynchException("Write sync lost.");
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
                buffer[writeHead] = inputLeft[offset];
                bufferRight[writeHead++] = inputRight[offset++];
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
                buffer[writeHead] = inputLeft[offset];
                bufferRight[writeHead++] = inputRight[offset++];
            }
            size_t count1 = end - buffer.size();
            writeHead = 0;
            for (size_t i = 0; i < count1; ++i)
            {
                buffer[writeHead] = inputLeft[offset];
                bufferRight[writeHead++] = inputRight[offset++];
            }
            count -= thisTime;
            this->writeHead = writeHead;
        }

        WRITE_BARRIER(); 
        this->atomicWriteCount += thisTime; // and release the reader (atomic operation)
        this->wWriteCount += thisTime;
    }
}

