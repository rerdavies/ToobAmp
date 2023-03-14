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

namespace LsNumerics {
    namespace CacheInfo {
        // not currently used.

        // Rasberry Pi 4
        constexpr size_t L1InstructionCacheSize = 192*1024;
        constexpr size_t L1InstructionCacheAssociativity = 3; 
        constexpr size_t L1InstructionBlockSize = L1InstructionCacheSize/L1InstructionCacheAssociativity;

        constexpr size_t L1DataCacheSize = 128*1024;
        constexpr size_t L1DataCacheAssociativity = 4;
        constexpr size_t L1DataBlockSize = L1DataCacheSize/L1DataCacheAssociativity; // 2-way associative

        constexpr size_t L2CacheSize = 1024*1024;
        constexpr size_t L2CacheAssociativity = 16;
        constexpr size_t L2BlockSize = L2CacheAssociativity/16; // 16-way associative.
    }
}