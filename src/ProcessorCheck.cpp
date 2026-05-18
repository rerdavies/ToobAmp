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

#include "ProcessorCheck.hpp"
#include <string.h>
#include <array>
#include <stdexcept>


#ifndef TOOB_OPTIMIZATION_FLAGS
#define TOOB_OPTIMIZATION_FLAGS ""
#endif
using namespace toob;

#ifdef __x86_64__

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
        f.avx = (ecx >> 28) & 1;

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
        f.avx512f = (ebx >> 16) & 1;
        f.avx512dq = (ebx >> 17) & 1;
        f.avx512cd = (ebx >> 28) & 1;
        f.avx512bw = (ebx >> 30) & 1;
        f.avx512vl = (ebx >> 31) & 1;
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

CpuLevel toob::GetCpuLevel()
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

void toob::ProcessorCheck()
{
    CpuLevel cpuLevel = GetCpuLevel();

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

#ifdef __aarch64__

void toob::ProcessorCheck()
{
}

#endif
