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
#include <cstddef>
#include <vector>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <complex>
#include <condition_variable>


namespace LsNumerics {



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


    class LocklessQueue
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

        LocklessQueue(bool isStereo, size_t size, size_t lowWaterMark = DEFAULT_LOW_WATER_MARK)
        {
            SetSize(isStereo,size, lowWaterMark);
        }
        LocklessQueue()
            : LocklessQueue(false,0, 0) 
        {
        }
        ~LocklessQueue()
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

        void SetSize(bool isStereo, size_t size, size_t lowWaterMark = DEFAULT_LOW_WATER_MARK)
        {
            this->isStereo = isStereo;
            if (lowWaterMark == DEFAULT_LOW_WATER_MARK)
            {
                lowWaterMark = size / 2;
            }
            this->lowWaterMark = lowWaterMark+MAX_READ_BORROW;
            if (size != 0)
            {
                buffer.resize(size+ MAX_READ_BORROW);
                if (isStereo)
                {
                    bufferRight.resize(size+ MAX_READ_BORROW);
                }
            }
        }

        void Close()
        {
            atomicClosed = true;
            writeStalled = false;
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
        void Read(float*left, float*right)
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
            *left = buffer[readHead];
            *right = bufferRight[readHead++];
            if (readHead == buffer.size())
            {
                readHead = 0;
            }
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

        // void WriteWait()
        // {
        //     std::unique_lock lock{mutex};
        //     writeStalled = true;
        //     this->readToWriteConditionVariable.wait(lock);
        // }

        void Write(size_t count, size_t offset, const std::vector<float> &input);
        void Write(size_t count, size_t offset, const std::vector<std::complex<double>> &input);
        void Write(size_t count, size_t offset, const std::vector<float> &inputLeft, const std::vector<float>&inputRight);
        void Write(size_t count, size_t offset, const std::vector<std::complex<double>> &inputLeft,const std::vector<std::complex<double>> &inputRight);

        size_t GetReadWaits()
        {
            size_t result = readWaits;
            readWaits = 0;
            return result;
        }

    private:
        bool isStereo = false;
        std::atomic<bool> writeStalled = false;
        std::atomic<uint32_t> atomicWriteCount = 0;
        uint32_t rWriteCount = 0;
        uint32_t wWriteCount = 0;
        std::atomic<bool> atomicClosed = false;

        std::size_t readWaits = 0;
        std::uint32_t writeHead = 0;
        std::uint32_t readHead = 0;
        std::uint32_t readCount = 0;
        std::uint32_t borrowedReads = 0;
        std::uint32_t lowWaterMark = 0;

        std::vector<float> buffer;
        std::vector<float> bufferRight;
    };

}