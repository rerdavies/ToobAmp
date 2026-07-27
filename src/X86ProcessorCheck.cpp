/*
 *   Copyright (c) 2026 Robin E. R. Davies
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

 #ifdef __x86_64__

#include "X86ProcessorCheck.hpp"
#include <string.h>
#include <array>
#include <cstdint>
#include <stdexcept>


#ifndef TOOB_OPTIMIZATION_FLAGS
#define TOOB_OPTIMIZATION_FLAGS ""
#endif
using namespace toob;


#ifdef _MSC_VER
#include <intrin.h>
static void cpuid(int info[4], int leaf, int subleaf = 0)
{
    __cpuidex(info, leaf, subleaf);
}
#else
#include <cpuid.h>
static void cpuid(int info[4], int leaf, int subleaf = 0)
{
    __cpuid_count(leaf, subleaf, info[0], info[1], info[2], info[3]);
}
#endif

namespace
{
    struct CpuFeatures
    {
        // x86-64-v2
        bool cx16 = false;   // CMPXCHG16B
        bool lahf = false;   // LAHF/SAHF in 64-bit mode
        bool popcnt = false; // POPCNT
        bool sse3 = false;
        bool sse4_1 = false;
        bool sse4_2 = false;
        bool ssse3 = false;

        // x86-64-v3
        bool avx = false;
        bool avx2 = false;
        bool bmi1 = false;
        bool bmi2 = false;
        bool f16c = false;
        bool fma = false;
        bool lzcnt = false;
        bool movbe = false;
        bool osxsave = false;

        // x86-64-v4
        bool avx512f = false;
        bool avx512bw = false;
        bool avx512cd = false;
        bool avx512dq = false;
        bool avx512vl = false;
    };

    // Read XCR0 via XGETBV to check which XSAVE state components the OS has enabled.
    // Only call this when OSXSAVE is already known to be set (i.e. CR4.OSXSAVE=1),
    // otherwise XGETBV will fault.
    static uint64_t read_xcr0()
    {
#ifdef _MSC_VER
        return _xgetbv(0);
#else
        uint32_t eax, edx;
        __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
        return (static_cast<uint64_t>(edx) << 32) | eax;
#endif
    }
}

static CpuFeatures detect_cpu_features()
{
    CpuFeatures f;
    std::array<int, 4> info{};

    // --- Leaf 0: max standard leaf ---
    cpuid(info.data(), 0);
    int max_leaf = info[0];

    // --- Leaf 1: basic features ---
    if (max_leaf >= 1)
    {
        cpuid(info.data(), 1);
        int ecx = info[2];
        int edx = info[3];

        f.sse3 = (ecx >> 0) & 1;
        f.ssse3 = (ecx >> 9) & 1;
        f.cx16 = (ecx >> 13) & 1;
        f.sse4_1 = (ecx >> 19) & 1;
        f.sse4_2 = (ecx >> 20) & 1;
        f.popcnt = (ecx >> 23) & 1;
        f.movbe = (ecx >> 22) & 1;
        f.f16c = (ecx >> 29) & 1;
        f.fma = (ecx >> 12) & 1;
        f.osxsave = (ecx >> 27) & 1;
        bool cpuid_avx = (ecx >> 28) & 1;

        // AVX requires the OS to have enabled YMM state (XCR0 bits 1 and 2).
        // OSXSAVE tells us XGETBV is safe to call; we must still verify XCR0.
        if (f.osxsave && cpuid_avx)
        {
            uint64_t xcr0 = read_xcr0();
            constexpr uint64_t XCR0_XMM = 1ULL << 1;
            constexpr uint64_t XCR0_YMM = 1ULL << 2;
            f.avx = (xcr0 & (XCR0_XMM | XCR0_YMM)) == (XCR0_XMM | XCR0_YMM);
        }

        (void)edx; // SSE/SSE2 guaranteed by x86-64 baseline
    }

    // --- Leaf 7: extended features ---
    if (max_leaf >= 7)
    {
        cpuid(info.data(), 7, 0);
        int ebx = info[1];
        int ecx = info[2];
        (void)ecx;

        f.bmi1 = (ebx >> 3) & 1;
        f.avx2 = (ebx >> 5) & 1;
        f.bmi2 = (ebx >> 8) & 1;

        // AVX-512 also requires the OS to have enabled opmask + ZMM state (XCR0 bits 5,6,7).
        // Only check if AVX is already confirmed (which already validated OSXSAVE + XCR0 bits 1,2).
        bool cpuid_avx512f = (ebx >> 16) & 1;
        if (f.avx && cpuid_avx512f)
        {
            uint64_t xcr0 = read_xcr0();
            constexpr uint64_t XCR0_OPMASK  = 1ULL << 5;
            constexpr uint64_t XCR0_ZMM_HI  = 1ULL << 6;
            constexpr uint64_t XCR0_HI16_ZMM = 1ULL << 7;
            if ((xcr0 & (XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16_ZMM)) ==
                        (XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16_ZMM))
            {
                f.avx512f  = true;
                f.avx512dq = (ebx >> 17) & 1;
                f.avx512cd = (ebx >> 28) & 1;
                f.avx512bw = (ebx >> 30) & 1;
                f.avx512vl = (ebx >> 31) & 1;
            }
        }
    }

    // --- Extended leaf 0x80000001: LAHF/SAHF, LZCNT ---
    cpuid(info.data(), 0x80000000);
    int max_ext_leaf = info[0];

    if (max_ext_leaf >= (int)0x80000001)
    {
        cpuid(info.data(), 0x80000001);
        int ecx = info[2];

        f.lahf = (ecx >> 0) & 1;  // LAHF/SAHF
        f.lzcnt = (ecx >> 5) & 1; // LZCNT (ABM)
    }

    return f;
}

CpuLevel toob::GetX86CpuLevel()
{
    CpuFeatures f = detect_cpu_features();
    // x86-64-v4 requires all of v3 + AVX-512 subset
    if (f.avx512f && f.avx512bw && f.avx512cd && f.avx512dq && f.avx512vl)
    {
        // Also requires v3 features — fall through check below
        if (f.avx && f.avx2 && f.bmi1 && f.bmi2 && f.f16c && f.fma &&
            f.lzcnt && f.movbe && f.osxsave &&
            f.cx16 && f.lahf && f.popcnt &&
            f.sse3 && f.sse4_1 && f.sse4_2 && f.ssse3)
        {
            return CpuLevel::V4;
        }
    }

    // x86-64-v3
    if (f.avx && f.avx2 && f.bmi1 && f.bmi2 && f.f16c && f.fma &&
        f.lzcnt && f.movbe && f.osxsave &&
        f.cx16 && f.lahf && f.popcnt &&
        f.sse3 && f.sse4_1 && f.sse4_2 && f.ssse3)
    {
        return CpuLevel::V3;
    }

    // x86-64-v2
    if (f.cx16 && f.lahf && f.popcnt &&
        f.sse3 && f.sse4_1 && f.sse4_2 && f.ssse3)
    {
        return CpuLevel::V2;
    }
    return CpuLevel::V1;
}

// void print_features(const CpuFeatures &f, int level)
// {
//     auto ok = [](bool v)
//     { return v ? "✓" : "✗"; };

//     std::cout << "=== x86-64 Microarchitecture Level: v" << level << " ===\n\n";

//     std::cout << "-- x86-64-v2 features --\n";
//     std::cout << "  LAHF/SAHF : " << ok(f.lahf) << "\n";
//     std::cout << "  CMPXCHG16B: " << ok(f.cx16) << "\n";
//     std::cout << "  POPCNT    : " << ok(f.popcnt) << "\n";
//     std::cout << "  SSE3      : " << ok(f.sse3) << "\n";
//     std::cout << "  SSSE3     : " << ok(f.ssse3) << "\n";
//     std::cout << "  SSE4.1    : " << ok(f.sse4_1) << "\n";
//     std::cout << "  SSE4.2    : " << ok(f.sse4_2) << "\n";

//     std::cout << "\n-- x86-64-v3 features --\n";
//     std::cout << "  AVX       : " << ok(f.avx) << "\n";
//     std::cout << "  AVX2      : " << ok(f.avx2) << "\n";
//     std::cout << "  BMI1      : " << ok(f.bmi1) << "\n";
//     std::cout << "  BMI2      : " << ok(f.bmi2) << "\n";
//     std::cout << "  F16C      : " << ok(f.f16c) << "\n";
//     std::cout << "  FMA       : " << ok(f.fma) << "\n";
//     std::cout << "  LZCNT     : " << ok(f.lzcnt) << "\n";
//     std::cout << "  MOVBE     : " << ok(f.movbe) << "\n";
//     std::cout << "  OSXSAVE   : " << ok(f.osxsave) << "\n";

//     std::cout << "\n-- x86-64-v4 features --\n";
//     std::cout << "  AVX-512F  : " << ok(f.avx512f) << "\n";
//     std::cout << "  AVX-512BW : " << ok(f.avx512bw) << "\n";
//     std::cout << "  AVX-512CD : " << ok(f.avx512cd) << "\n";
//     std::cout << "  AVX-512DQ : " << ok(f.avx512dq) << "\n";
//     std::cout << "  AVX-512VL : " << ok(f.avx512vl) << "\n";
// }

void toob::X86ProcessorCheck()
{
    CpuLevel cpuLevel = GetX86CpuLevel();

    bool valid;
    if (strcmp(TOOB_OPTIMIZATION_FLAGS, "x86-64-v2") == 0)
    {
        valid = cpuLevel >= CpuLevel::V2;
    }
    else if (strcmp(TOOB_OPTIMIZATION_FLAGS, "x86-64-v3") == 0)
    {
        valid = cpuLevel >= CpuLevel::V3;
    }
    else if (strcmp(TOOB_OPTIMIZATION_FLAGS, "x86-64-v4") == 0)
    {
        valid = cpuLevel >= CpuLevel::V4;
    }
    else
    {
        valid = true;
    }

    if (!valid) 
    {
        throw new std::runtime_error("CPU Not supported. (Compiled for " TOOB_OPTIMIZATION_FLAGS ")");
    }
}

#endif

