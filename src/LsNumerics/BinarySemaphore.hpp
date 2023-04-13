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

#include <stdexcept>>
#include <semaphore.h>
#include <chrono>

namespace LsNumerics
{
    // Temporary Repacement for std::semaphore. Replace with c++20 std::semaphore if you have access to c++20.

    class SemaphoreException : std::logic_error
    {
    public:
        SemaphoreException(int errorResult) : errorResult(errorResult), std::logic_error("Invalid semaphore operation.") {}
        int GetErrorResult() const { return errorResult; }

    private:
        int errorResult;
    };

#ifdef __cpp_lib_semaphore
    using template <std::ptrdiff_t LEAST_MAX_VALUE>
    CountingSemaphore = std::semaphore<LEAST_MAX_VALUE>;

    BinaryCountingSemaphore = std::binary_semaphore;

#else

    template <std::ptrdiff_t LeastMaxValue>
    class CountingSemaphore
    {
    public:
        CountingSemaphore(std::ptrdiff_t desired)
        {
            int result = sem_init(nullptr, 0, desired);
            if (result != 0) {
                try {
                    throw std::logic_exception("Failed to create semaphore.");
                } catch (const std::execption &e)
                {
                    ::terminate(e.what());
                }
                
            }
        }
        ~CountingSemaphore() {
            sem_destroy(&_sem);
        }
        void release();
        bool try_aquire();
        void aquire();
        template <class Rep, class Period>
        bool try_acquire_for(const std::chrono::duration<Rep, Period> &rel_time);

    private:
        sem_t _sem;
    };

    using BinarySemaphore = CountingSemaphore<1>;

    template <std::ptrdiff_t LeastMaxValue>
    inline CountingSemaphore<LeastMaxValue>::CountingSemaphore(std::ptrdiff_t desired)
    {
        int result = sem_init(nullptr, 0, desired);
        if (result != 0)
            throw SemaphoreException(result);
    }
    template <std::ptrdiff_t LeastMaxValue>
    inline void CountingSemaphore<LeastMaxValue>::release()
    {
        int result = sem_post(&_sem);
    }
    template <std::ptrdiff_t LeastMaxValue>
    inline bool CountingSemaphore<LeastMaxValue>::try_aquire()
    {
        while (true)
        {
            int result = sem_trywait(&_sem);
            if (result == 0)
                return true;
            if (result == EAGAIN)
                return false;
            if (result == EINTR)
            {
                continue;
            }
            throw SemaphoreException(result);
        }
    }
    template <std::ptrdiff_t LeastMaxValue>
    inline void CountingSemaphore<LeastMaxValue>::aquire()
    {
        while (true)
        {
            int result = sem_wait(&_sem);
            if (result == 0)
                return;
            if (result == EINTR)
                continue;
            throw SemaphoreException(result);
        }
    }

    template <std::ptrdiff_t LeastMaxValue,class Rep, class Period>
    inline bool CountingSemaphore<LeastMaxValue>::try_acquire_for(const std::chrono::duration<Rep, Period> &rel_time)
    {
        return false;
    }

#endif

}