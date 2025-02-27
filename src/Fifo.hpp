/*
 *   Copyright (c) 2025 Robin E. R. Davies
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

#include <cstdint>
#include <array>
#include <stdexcept>

namespace toob
{
    template <typename T, size_t N>
    class Fifo
    {
    public:
        void push_back(const T &value)
        {
            if (count == N)
            {
                throw std::runtime_error("Fifo is full");
            }
            buffer[tail] = value;
            tail = (tail + 1) % N;
            count++;
        }
        void push_back(T &&value)
        {
            if (count == N)
            {
                throw std::runtime_error("Fifo is full");
            }
            buffer[tail] = std::move(value);
            tail = (tail + 1) % N;
            count++;
        }
        T pop_front()
        {
            if (count == 0)
            {
                throw std::runtime_error("Fifo is empty");
            }
            T value = buffer[head];
            buffer[head] = T();
            head = (head + 1) % N;
            count--;
            return value;
        }

        size_t size() const
        {
            return count;
        }
        bool empty() const {
            return count == 0;
        }

        T &front() {
            return buffer[head];
        }

    private:
        std::array<T, N> buffer;
        size_t head = 0;
        size_t tail = 0;
        size_t count = 0;
    };
};