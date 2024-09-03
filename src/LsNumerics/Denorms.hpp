/*
 *   Copyright (c) 2024 Robin E. R. Davies
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
#include <cstdint>

#if defined(__aarch64__) || defined(__ARM_ARCH_ISA_A64) || defined(_M_ARM64) // clang, gcc, MSVC, respectively

// ARM 64 implementation
namespace LsNumerics
{
    using fp_state_t = uint64_t;
    fp_state_t disable_denorms();
    void restore_denorms(fp_state_t);
}

#elif defined(__x86_64__) || (defined(_M_X64) || defined(_M_AMD64)) // clang/gcc and msvc respectively.

// x64 implemetnation
#include <xmmintrin.h>
#include <float.h>

namespace LsNumerics
{
    using fp_state_t = unsigned int;

    inline fp_state_t disable_denorms()
    {
        unsigned int current_word, new_word;
        _controlfp_s(&current_word, 0, 0);
        new_word = current_word | _DN_FLUSH;
        _controlfp_s(&current_word, new_word, _MCW_DN);
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        return current_word;
    }
    inline void restore_denorms(fp_state_t originalState)
    {
        unsigned int unused;
        _controlfp_s(&unused, originalState, _MCW_DN );
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_OFF);

    }
}
#else
#error Platform not supported (buf if you add a platform, please let me know)
#endif
