/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#pragma once
#include <vector>
#include <cstddef>
#include <cassert>

namespace toob {
    template<typename T>
    class CircularBuffer {
    public:
        CircularBuffer() {
            SetSize(0);
        }
        CircularBuffer(size_t size)
        {
            SetSize(size);
        }

        bool Overrun() const { return overrun; }
        void Overrun(bool value) { overrun = value;}
        void Reset() {
            overrun = false;
            head = 0; count = 0;
        }
        void SetSize(size_t size)
        {
            buffer.resize(size);
            Reset();
        }
        void Add(const T &value)
        {
            if (count == buffer.size())
            {
                if (locked) {
                    overrun = true;
                    return;
                }
            } else {
                ++count;
            }
            buffer[head++] = value;
            if (head == buffer.size())
            {
                head = 0;
            }
        }
        class LockResult;

        class ReadIterator {
        public:
            ReadIterator(const LockResult*lockResult, T*p)
            :   lockResult(lockResult),
                p(p)
            {
            }
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            bool operator==(const ReadIterator&other)
            {
                return p == other.p;
            }
            bool operator!=(const ReadIterator&other)
            {
                return p != other.p;
            }
            const T operator*() const { return *p; }
            const T* operator->() const { return p; }



            T&operator*() { return *p; }
            // pre-increment.
            ReadIterator&operator++()
            {
                ++p;
                if (p == lockResult->p0+lockResult->count0)
                {
                    p = lockResult->p1;
                }
                return *this;
            }
            // post-increment
            ReadIterator operator++(int)
            {
                ReadIterator t(*this);
                operator++();
                return t;
            }

        private:
            const LockResult *lockResult;
            T*p;
        };
        struct LockResult {
            T * p0;
            size_t count0;
            T* p1;
            size_t count1;

            ReadIterator begin() const {
                return ReadIterator(this,p0);
            }
            ReadIterator end() const {
                return ReadIterator(this,p1+count1);
            }
            using iterator = ReadIterator;
        };
        LockResult Lock(size_t count) {
            assert(count <= this->count);
            locked = true;
            LockResult result;
            this->count = count;
            size_t readTail = this->head;
            if (count > readTail)
            {
                result.count0 = count-readTail;
                result.p0 = &(buffer[buffer.size()-result.count0]);
                result.p1 = &(buffer[0]);
                result.count1 = count-result.count0;
            } else {
                result.p0 = &buffer[readTail-count];
                result.count0 = count;
                result.p1 = result.p0+result.count0; // for a useful iterator end()
                result.count1 = 0;
            }
            return result;
        }
        void Unlock() {
            locked = false;
            if (overrun)
            {
                // just start over.
                Reset();
            }
        }
        size_t Size() const { return count;}
    private:
        bool locked = false;
        bool overrun = false;
        std::vector<T> buffer;
        volatile size_t head, count;
    };
} // namespace