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

#include "StagedFft.hpp"
#include "CacheInfo.hpp"
#include <iostream>
#include "LsMath.hpp"

using namespace LsNumerics;
using namespace LsNumerics::Implementation;

static size_t log2(size_t x)
{
    size_t result = 0;

    while (x > 1)
    {
        x >>= 1;
        ++result;
    }

    return result;
}

/*
 *  Bit reverse an integer given a word of nb bits
 *  NOTE: Only works for 32-bit words max
 *  examples:
 *  10b      -> 01b
 *  101b     -> 101b
 *  1011b    -> 1101b
 *  0111001b -> 1001110b
 */
static uint32_t BitReverse(uint32_t value, size_t bits)
{
    size_t result = 0;
    for (size_t i = 0; i < bits; ++i)
    {
        result = (result << 1) | (value & 1);
        value >>= 1;
    }
    return result;
}

static inline StagedFftPlan::complex_t Wn(size_t i, size_t n, StagedFftPlan::Direction dir)
{
    return std::exp(StagedFftPlan::complex_t(0, 2 * LsNumerics::Pi * i / n * (double)dir));
}

void StagedFftPlan::TransposeOutputs(InstanceData &instanceData,size_t cacheSize, size_t size, const VectorRange<complex_t> &outputs, Direction dir)
{
    std::vector<complex_t>& tmpBuff = instanceData.GetWorkingBuffer();
    size_t outputIndex = 0;
    complex_t w = complex_t(1, 0);
    complex_t wRot = Wn(1, size, dir);

    for (size_t i = 1; i < size / 2; ++i)
    {
        tmpBuff[outputIndex++] = outputs[i];
        tmpBuff[outputIndex++] = outputs[i + size / 2] * w;
        w *= wRot;
    }
    for (size_t i = 0; i < outputs.size(); ++i)
    {
        outputs[i] = tmpBuff[i];
    }
}

void StagedFftPlan::SetSize(size_t size)
{

    ops.resize(0);
    if (this->fftSize == size)
    {
        return;
    }
    assert((size & (size - 1)) == 0); // must be power of 2!

    this->fftSize = size;
    bitReverse.resize(fftSize);

    log2N = log2(fftSize);

    for (size_t j = 0; j < fftSize; ++j)
    {
        bitReverse[j] = BitReverse(j, log2N);
    }

    // create data for doing in-place bit-reversal of input buffer.
    reverseBitPairs.resize(0);
    reverseBitSelfPairs.resize(0);
    for (size_t i = 0; i < bitReverse.size(); ++i)
    {
        if (i == bitReverse[i])
        {
            reverseBitSelfPairs.push_back(i);
        }
        else if (i < bitReverse[i])
        {
            reverseBitPairs.push_back(std::pair<uint32_t, uint32_t>(bitReverse[i], i));
            assert(i == bitReverse[bitReverse[i]]);
        }
    }
    norm = double(1 / std::sqrt(double(fftSize)));
    CalculateTwiddleFactors(Direction::Forward, forwardTwiddle);
    CalculateTwiddleFactors(Direction::Backward, backwardTwiddle);



    // Exploit fast operation when a sub-DFT fits within an  L1 or L2 cache block. 
    // 
    // e.g. For DFT(2048), execute 4 sub-DFT(512)s sequentially in their entirety, instead of doing pass-by-pass on the full 2048 width.
    //
    // The optimization is performed for L2 blocks as well as L1 blocks.

    // DFTs smaller than one cache block operate within a single cache page, so they should get a huge performance boost.
    // 9 steps in the DFT(512) operate in a single cache block, on Pi 4, so no fetches for partial cache lines occur,
    // and there's a significant opportunity for writes in subsequent passes to discard pending writes. The same argument extends
    // to L2 blocks, where executing sub-DFTs in their entirety avoids spilling the L2 cache.
    //
    // We should never saturate L2 caches in practice.


    constexpr size_t maxL2CacheSize = CacheInfo::L2CacheSize/2;
    constexpr size_t l2CacheFftSize = maxL2CacheSize / (sizeof(complex_t));
    size_t l2Log2CacheSize = log2(l2CacheFftSize);

    constexpr size_t maxL1CacheSize = CacheInfo::L1DataBlockSize;
    constexpr size_t l1CacheFftSize = maxL1CacheSize / (sizeof(complex_t));
    size_t l1Log2CacheSize = log2(l1CacheFftSize);


    if (this->log2N > l2Log2CacheSize)
    {

        isL2Optimized = true;
        cacheEfficientFft = &GetCachedInstance(l2CacheFftSize);
        size_t currentPass = 1;

        ops.push_back(
            [this, l2CacheFftSize](InstanceData &instanceData, const VectorRange<complex_t> &outputs, Direction dir)
            {
                size_t size = this->GetSize();
                for (size_t i = 0; i < size; i += l2CacheFftSize)
                {
                    auto subRange = VectorRange<complex_t>(i, i + l2CacheFftSize, outputs);
                    cacheEfficientFft->ComputeInner(instanceData, subRange, dir);
                }
            });
        currentPass += l2Log2CacheSize;
        

        for (size_t pass = currentPass; pass <= log2N; ++pass)
        {
            ops.push_back(
                [this, pass](InstanceData &instanceData, const VectorRange<complex_t> &outputs, Direction dir)
                {
                    ComputePass(pass, outputs, dir);
                });

        }
    } else if (log2N > l1Log2CacheSize)
    {
        // Perfor L1 cache optimization.
        //std::cout << "L1 CACHE OPTIMIZATION USED n=" << this->fftSize << std::endl;
        isL1Optimized = true;
        cacheEfficientFft = &GetCachedInstance(l1CacheFftSize);
        size_t currentPass = 1;
        ops.push_back(
            [this, l1CacheFftSize](InstanceData &instanceData, const VectorRange<complex_t> &outputs, Direction dir)
            {
                size_t size = this->GetSize();
                for (size_t i = 0; i < size; i += l1CacheFftSize)
                {
                    auto subRange = VectorRange<complex_t>(i, i + l1CacheFftSize, outputs);
                    cacheEfficientFft->ComputeInner(instanceData, subRange, dir);
                }
            });
        currentPass += l1Log2CacheSize;

        for (size_t pass = currentPass; pass <= log2N; ++pass)
        {
            ops.push_back(
                [this, pass](InstanceData &instanceData, const VectorRange<complex_t> &outputs, Direction dir)
                {
                    ComputePass(pass, outputs, dir);
                });

        }

    }
    else
    {
        if (log2N > 0) // DFT(1) is do noting. So do nothing.
        {
            // compute first step (no multiply, not pairwise)
            FftOp op =
                [this](InstanceData &instanceData,const VectorRange<complex_t> &outputs, Direction dir)
            {
                ComputeInner0(outputs, dir);
            };

            ops.push_back(
                std::move(op));
        }
        for (size_t pass = 2; pass <= log2N; ++pass)
        {
            FftOp op =
                [this, pass](InstanceData &instanceData,const VectorRange<complex_t> &outputs, Direction dir)
            {
                ComputePass(pass, outputs, dir);
            };

            ops.push_back(
                std::move(op));
        }
    }
}

void StagedFftPlan::Compute(InstanceData &instanceData, const std::vector<complex_t> &input, std::vector<complex_t> &output, StagedFftPlan::Direction dir)
{
    assert(fftSize != UNINITIALIZED_VALUE);
    assert(input.size() >= fftSize);
    assert(output.size() >= fftSize);

    size_t cnt = fftSize;

    if (&input == &output)
    {
        // in-place reverse.
        for (const auto &t : reverseBitPairs)
        {
            auto swap = output[t.first];
            output[t.first] = output[t.second] * norm;
            output[t.second] = swap * norm;
        }
        for (auto i : reverseBitSelfPairs)
        {
            output[i] *= norm;
        }
    }
    else
    {
        for (size_t j = 0; j < cnt; ++j)
        {
            output[j] = norm * input[bitReverse[j]];
        }
    }
    VectorRange<complex_t> t{output};
    ComputeInner(instanceData, t, dir);
}
void StagedFftPlan::Compute(InstanceData &instanceData, const std::vector<float> &input, std::vector<complex_t> &output, Direction dir)
{
    assert(fftSize != UNINITIALIZED_VALUE);
    assert(input.size() >= fftSize);
    assert(output.size() >= fftSize);

    for (size_t i = 0; i < fftSize; ++i)
        output[i] = norm * input[bitReverse[i]];

    ComputeInner(instanceData, output, dir);
}

void StagedFftPlan::ComputeInner(InstanceData &instanceData, const VectorRange<complex_t> &output, Direction dir)
{
    for (auto &op : ops)
    {
        op(instanceData, output, dir);
    }
}

void StagedFftPlan::CalculateTwiddleFactors(Direction dir, std::vector<complex_t> &twiddles)
{
    twiddles.resize(log2N + 1);
    for (size_t pass = 1; pass <= log2N; ++pass)
    {
        size_t groupSize = 1 << pass;          // butterfly mask
        size_t twiddleOffset = groupSize >> 1; // butterfly width
        // fft butterflies

        twiddles[pass] =
            std::exp(complex_t(0, Pi / twiddleOffset * double(dir)));
    }
}

void StagedFftPlan::ComputeInner0(const VectorRange<complex_t> &output, Direction dir)
{
    constexpr size_t pass = 1;
    constexpr size_t offset = 1 << pass;    // butterfly mask
    constexpr size_t mStride = offset >> 1; // butterfly width

    // fft butterflies
    for (size_t i = 0; i < mStride; ++i)
    {
        for (size_t k = i; k < fftSize; k += offset)
        {
            complex_t iLeft = output[k];
            complex_t iRight = output[k + mStride];
            output[k] = iLeft + iRight;
            output[k + mStride] = iLeft - iRight;
        }
    }
}
void StagedFftPlan::ComputePass(size_t pass, const VectorRange<complex_t> &output, Direction dir)
{
    // For small sections, do butterflies in the most compute-efficient order.
    size_t groupSize = 1 << pass;          // butterfly mask
    size_t twiddleOffset = groupSize >> 1; // butterfly width

    complex_t wj(1, 0);
    // complex_t wInc = std::exp(complex_t(0, Pi / twiddleOffset * double(dir)));
    std::vector<complex_t> &twiddleFactors = (dir == Direction::Forward) ? this->forwardTwiddle : this->backwardTwiddle;
    complex_t wInc = twiddleFactors[pass];

    // fft butterflies, 2 at a time in order to encourage use of f64x2 SIMD instructions.
    for (size_t j = 0; j < twiddleOffset; j += 2)
    {
        complex_t wj2 = wj * wInc;
        for (size_t k = j; k < fftSize; k += groupSize)
        {
            complex_t *RESTRICT pLeft0;
            complex_t *RESTRICT pRight0;
            complex_t *RESTRICT pLeft1;
            complex_t *RESTRICT pRight1;

            pLeft0 = &(output[k]);
            pRight0 = &(output[k + twiddleOffset]);

            complex_t iLeft = *pLeft0;
            complex_t iRight = wj * (*pRight0);
            *pLeft0 = iLeft + iRight;
            *pRight0 = iLeft - iRight;

            size_t k2 = k + 1;
            pLeft1 = &output[k2];
            pRight1 = &output[k2 + twiddleOffset];
            complex_t iLeft2 = output[k2];
            complex_t iRight2 = wj2 * output[k2 + twiddleOffset];
            *pLeft1 = iLeft2 + iRight2;
            *pRight1 = iLeft2 - iRight2;
        }
        wj = wj2 * wInc;
    }
}

std::recursive_mutex StagedFftPlan::cacheMutex;
std::vector<std::unique_ptr<StagedFftPlan>> StagedFftPlan::cache(64);

StagedFftPlan &StagedFftPlan::GetCachedInstance(size_t size)
{

    std::lock_guard<std::recursive_mutex> lock{cacheMutex};

    int log2Size = log2(size);
    if (!cache[log2Size])
    {
        cache[log2Size] = std::unique_ptr<StagedFftPlan>{new StagedFftPlan(size)};
    }
    return *(cache[log2Size].get());
}

// original unoptimized code.
//         void ComputeInner(std::vector<complex_t> &output, fft_dir dir)
// {
//     // fft passes
//     // pass zero case. w is always 1.
//     {
//         constexpr size_t pass = 1;
//         constexpr size_t mOffset = 1 << pass;  // butterfly mask
//         constexpr size_t mStride = mOffset >> 1; // butterfly width

//         // fft butterflies
//         for (size_t j = 0; j < mStride; ++j)
//         {
//             for (size_t k = j; k < fftSize; k += mOffset)
//             {
//                 complex_t iLeft = output[k];
//                 complex_t iRight = output[k + mStride];
//                 output[k] = iLeft + iRight;
//                 output[k + mStride] = iLeft - iRight;
//             }
//         }

//     }

//     for (size_t pass = 2; pass <= log2N; ++pass)
//     {
//         size_t mOffset = 1 << pass;  // butterfly mask
//         size_t mStride = mOffset >> 1; // butterfly width

//         complex_t wj(1, 0);
//         complex_t wInc = std::exp(complex_t(0, Pi / mStride * double(dir)));

//         // fft butterflies, 2 at a time in order to encourage use of f64x2 SIMD instructions.
//         for (size_t j = 0; j < mStride; ++j)
//         {
//             complex_t w = complex_t((double)wj.real(), (double)wj.imag());
//             for (size_t k = j; k < fftSize; k += mOffset)
//             {
//                 complex_t iLeft = output[k];
//                 complex_t iRight = w * output[k + mStride];
//                 output[k] = iLeft + iRight;
//                 output[k + mStride] = iLeft - iRight;
//             }
//             wj *= wInc;
//         }
//     }
// }
