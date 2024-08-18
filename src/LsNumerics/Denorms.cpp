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
#include "Denorms.hpp"

using namespace LsNumerics;

#if defined(__aarch64__) || defined(__ARM_ARCH_ISA_A64) || defined(_M_ARM64) // clang, gcc, MSVC, respectively
fp_state_t LsNumerics::disable_denorms()
{
    uint64_t original_fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(original_fpcr));
    uint64_t new_fpcr = original_fpcr | (1ULL << 24); // Set the FZ (Flush-to-Zero) bit
    __asm__ volatile("msr fpcr, %0" : : "r"(new_fpcr));
    return original_fpcr;
}
void LsNumerics::restore_denorms(fp_state_t originalValue)
{
    __asm__ volatile("msr fpcr, %0" : : "r"(originalValue));
}
#endif
