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
#include <atomic>

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

        void Reset() {
            for (size_t i = 0; i < buffer.size(); ++i)
            {
                buffer[i] = 0;
            }
            head = 0;
        }
        void SetSize(size_t size)
        {
            buffer.resize(size);
            Reset();
        }
        void Add(const T &value)
        {
            buffer[head++] = value;
            if (head == buffer.size())
            {
                head = 0;
            }
        }

        void CopyTo(std::vector<float> &buffer) const {
            size_t count = buffer.size();
            if (head >= count)
            {
                size_t ix = 0;
                for (size_t i = head-count; i != head; ++i)
                {
                    buffer[ix++] = this->buffer[i];
                }
            } else {
                size_t ix = 0;
                size_t start = this->head + this->buffer.size()-count;
                for (size_t i = start; i < buffer.size(); ++i)
                {
                    buffer[ix++] = this->buffer[i]; 
                }
                for (size_t i = 0; i < this->head; ++i)
                {
                    buffer[ix++] = this->buffer[i];
                }
            }

        }
        size_t Size() const { return buffer.size();}
    private:
        std::vector<T> buffer;
        size_t head = 0;
    };
} // namespace