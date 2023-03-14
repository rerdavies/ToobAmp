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

namespace LsNumerics {


    /// @brief Single-writer multiple-reader delay line

    class SynchronizedDelayLine
    {
    public:
        class SynchException: public std::logic_error
        {
        public:
            SynchException(const std::string &message)
            :std::logic_error(message)
            {
            }
        };
        class ClosedException: public std::logic_error
        {
        public:
            ClosedException()
            :std::logic_error("Closed.")
            {
            }
        };



    public:
        SynchronizedDelayLine() { SetSize(0,0); }
        SynchronizedDelayLine(
            size_t size,
            size_t audioBufferSize // maximum number of times push can be called before synch is called.
            )
        {
            SetSize(size,audioBufferSize);
        }
        ~SynchronizedDelayLine();

        void SetSize(size_t size, size_t padEntries);

        void Write(float value)
        {
            ++head;
            storage[head] = value;
        }
        void SynchWrite()
        {
            std::lock_guard lock { mutex };
            readHead = head;
            readTail = head+size;
            conditionVariable.notify_all();
        }
        float At(size_t index) const
        {
            return storage[(head - index) & sizeMask];
        }

        float operator[](size_t index) const
        {
            return At(index);
        }

        void ReadLock(size_t position, size_t size);
        void ReadUnlock(size_t poisiont, size_t size);

        void ReadRange(size_t position, size_t size,size_t outputOffset, std::vector<float>&output);
        void ReadRange(size_t position, size_t count,std::vector<float>& output)
        {
            ReadRange(position,count,0,output);
        }

public:
        class ReadLock {
        public:
            ReadLock(SynchronizedDelayLine&delayLine, size_t position, size_t count)
            : delayLine(delayLine),position(position),count(count)
            {
                delayLine.ReadLock(position,count);
            }
            ~ReadLock()
            {
                delayLine.ReadUnlock(position,count);
            }
        private:
            SynchronizedDelayLine&delayLine;
            size_t position, count;
        };

        bool IsReadReady(size_t position, size_t count);
        void WaitForRead(size_t position, size_t count);

        void Close();

        void CreateThread(const std::function<void(void)>&threadProc, int relativeThreadPriority);

    private:
        bool closed = false;
        std::mutex mutex;
        std::condition_variable conditionVariable;
        std::vector<float> storage;
        std::size_t head = 0;
        std::size_t size = 0;
        std::size_t padEntries = 0;
        std::size_t sizeMask = 0;
        std::size_t readHead = 0;
        std::size_t readTail = 0;
        using thread_ptr = std::unique_ptr<std::thread>;

        std::vector<thread_ptr> threads;
    };
}


#endif //SYNCHRONIZED_DELAY_LINE_HPP