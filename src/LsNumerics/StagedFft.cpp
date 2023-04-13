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
#include <cassert>

static constexpr bool disableShuffleOptimization = true;

using namespace LsNumerics;
using namespace LsNumerics::Implementation;

using complex_t = std::complex<double>;

static constexpr size_t maxL2CacheSize = CacheInfo::L2CacheSize / 2;
static constexpr size_t l2CacheFftSize = maxL2CacheSize / (sizeof(complex_t));
static size_t l2Log2CacheSize = log2(l2CacheFftSize);

static constexpr size_t maxL1CacheSize = CacheInfo::L1DataBlockSize;
static constexpr size_t l1CacheFftSize = maxL1CacheSize / (sizeof(complex_t));
static size_t l1Log2CacheSize = log2(l1CacheFftSize);

static inline size_t pow2(size_t x)
{
    return 1 << x;
}
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

    // There are three straegies:
    // 1) execute sub-DFTs one by one, in order to exploit L1 cache.
    // 2) execute sub-DFTs one by one, in order to exploit L2 cache.
    // 3) Shuffle pass data to place data in order so that shuffled stages can be run with modified sub-DFTs
    //   that will fit in L1 cache again.
    //
    // Shuffling requires two extra passes that will destroy the L1 and L2 cache; so one has to choose carefully
    // whether to use shuffling or explot L2 cache, and take a beating on the last couple of stages. However,
    // there are huge advantages to operating in L1 cache, especially when FFTs are running concurrently.
    // Whether to use Shuffles, or L2 cache optimizations is a tuning decision.

    // DFTs smaller than one cache block operate within a single cache page, so they should get a huge performance boost.
    // 9 stages in the DFT(512) operate in a single cache block, on Pi 4, so no fetches for partial cache lines occur,
    // and there's a significant opportunity for writes in subsequent passes to discard pending writes. The same argument extends
    // to L2 blocks, where executing sub-DFTs in their entirety avoids spilling the L2 cache.
    //

    // size_t shuffleLog2CacheSize = l1Log2CacheSize*2;

    bool useShuffle = (!disableShuffleOptimization) && this->log2N > l1Log2CacheSize; // + 3;

    if (this->log2N > l2Log2CacheSize && !useShuffle)
    {

        isL2Optimized = true;
        cacheEfficientFft = &GetCachedInstance(l2CacheFftSize);
        size_t currentPass = 1;

        ops.push_back(
            [this](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
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
                [this, pass](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
                {
                    ComputePass(pass, outputs, dir);
                });
        }
    }
    else if (log2N > l1Log2CacheSize)
    {
        // Perform L1 cache optimization.
        // std::cout << "L1 CACHE OPTIMIZATION USED n=" << this->fftSize << std::endl;
        isL1Optimized = true;
        cacheEfficientFft = &GetCachedInstance(l1CacheFftSize);
        size_t currentPass = 1;
        ops.push_back(
            [this](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
            {
                size_t size = this->GetSize();
                for (size_t i = 0; i < size; i += l1CacheFftSize)
                {
                    auto subRange = VectorRange<complex_t>(i, i + l1CacheFftSize, outputs);
                    cacheEfficientFft->ComputeInner(instanceData, subRange, dir);
                }
            });
        currentPass += l1Log2CacheSize;

        if (useShuffle)
        {
            currentPass = AddShuffleOps(currentPass, fftSize);
        }
        // hammer out the last few passes (which will all fit in L2 cache)
        for (size_t pass = currentPass; pass <= log2N; ++pass)
        {
            if (pass > l1Log2CacheSize)
            {
                ops.push_back(
                    [this, pass](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
                    {
                        ComputePassLarge(pass, outputs, dir);
                    });
            }
            else
            {
                ops.push_back(
                    [this, pass](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
                    {
                        ComputePass(pass, outputs, dir);
                    });
            }
        }
    }
    else
    {
        if (log2N > 0) // DFT(1) is do noting. So do nothing.
        {
            // compute first step (no multiply, not pairwise)
            FftOp op =
                [this](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
            {
                ComputeInner0(outputs, dir);
            };

            ops.push_back(
                std::move(op));
        }
        for (size_t pass = 2; pass <= log2N; ++pass)
        {
            FftOp op =
                [this, pass](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
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

    VectorRange<complex_t> outputRange(output);
    ComputeInner(instanceData, outputRange, dir);
}

void StagedFftPlan::ComputeInner(InstanceData &instanceData, VectorRange<complex_t> &output, Direction dir)
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

void StagedFftPlan::ComputeInner0(VectorRange<complex_t> &output, Direction dir)
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

void StagedFftPlan::ComputePass(size_t pass, VectorRange<complex_t> &output, Direction dir)
{
    // For small sections, do butterflies in the most compute-efficient order.
    size_t groupSize = 1 << pass;          // butterfly mask
    size_t twiddleOffset = groupSize >> 1; // butterfly width

    complex_t wj(1, 0);
    // complex_t wInc = std::exp(complex_t(0, Pi / twiddleOffset * double(dir)));
    std::vector<complex_t> &twiddleFactors = (dir == Direction::Forward) ? this->forwardTwiddle : this->backwardTwiddle;
    complex_t wInc = twiddleFactors[pass];

    // TODO: Resync wj periodically for large FFTs.

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
void StagedFftPlan::ComputePassLarge(size_t pass, VectorRange<complex_t> &output, Direction dir)
{
    // same as ComputePass, but periodically re-syncs the value of wj in order
    // to prevent loss of precision.

    size_t groupSize = 1 << pass;          // butterfly mask
    size_t twiddleOffset = groupSize >> 1; // butterfly width

    complex_t wj(1, 0);
    // complex_t wInc = std::exp(complex_t(0, Pi / twiddleOffset * double(dir)));
    std::vector<complex_t> &twiddleFactors = (dir == Direction::Forward) ? this->forwardTwiddle : this->backwardTwiddle;
    complex_t wInc = twiddleFactors[pass];

    // TODO: Resync wj periodically for large FFTs.

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
        // prevent loss of precision in large DFTs.
        constexpr size_t RESYNCH_RATE = 512;
        if ((j & (RESYNCH_RATE - 1)) == 0 && j >= RESYNCH_RATE)
        {
            auto wjNew = std::exp(complex_t(0, j * Pi / twiddleOffset * double(dir)));

            assert(std::abs(wjNew - wj) <= 1E-10);

            wj = wjNew;
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

class InPlaceShuffle
{
public:
    InPlaceShuffle(const std::vector<uint32_t> &map)
    {

        this->map = map;

        MakeCycles(map, cycles);
    }
    InPlaceShuffle(const std::vector<uint32_t> &&map)
    {

        this->map = std::move(map);

        MakeCycles(map, cycles);
    }
    static InPlaceShuffle Identity(size_t size)
    {
        std::vector<uint32_t> v;
        v.resize(size);
        for (size_t i = 0; i < size; ++i)
        {
            v[i] = i;
        }
        return InPlaceShuffle(std::move(v));
    }

    size_t Size() const { return map.size(); }
    template <typename T>
    std::vector<T> Shuffle(const std::vector<T> &vector) const
    {
        std::vector<T> result;
        result.resize(vector.size());
        for (size_t i = 0; i < map.size(); ++i)
        {
            result[i] = vector[map[i]];
        }
        return result;
    }
    InPlaceShuffle Shuffle(const InPlaceShuffle &other)
    {
        if (other.Size() != this->Size())
        {
            throw std::invalid_argument("Not the same size.");
        }
        std::vector<uint32_t> mapResult;
        mapResult.resize(this->Size());

        for (size_t i = 0; i < Size(); ++i)
        {
            mapResult[i] = this->map[other.map[i]];
        }
        return InPlaceShuffle(std::move(mapResult));
    }
    uint32_t Map(size_t i) const
    {
        return map[i];
    }

    template <typename T>
    void ShuffleInPlace(T &vector) const
    {
        for (auto cycle : cycles)
        {
            auto lastValue = vector[cycle];
            auto x = cycle;
            while (true)
            {
                auto nextX = map[x];
                if (nextX == cycle)
                {
                    vector[x] = lastValue;
                    break;
                }
                vector[x] = vector[nextX];
                x = nextX;
            }
        }
    }
    InPlaceShuffle MakeInverse() const
    {
        std::vector<uint32_t> inverse;
        inverse.resize(map.size());

        for (size_t i = 0; i < map.size(); ++i)
        {
            inverse[map[i]] = i;
        }
        return InPlaceShuffle(std::move(inverse));
    }

private:
private:
    void MakeCycles(const std::vector<uint32_t> &map, std::vector<uint32_t> &cycles)
    {
        std::vector<bool> visited;
        visited.resize(map.size());

        for (size_t i = 0; i < map.size(); ++i)
        {
            if (map[i] != i)
            {
                if (!visited[i])
                {
                    size_t x = i;
                    cycles.push_back(x);
                    size_t cycleLength = 0;
                    while (true)
                    {
                        assert(!visited[x]);

                        visited[x] = true;
                        x = map[x];
                        if (x == i)
                        {
                            break;
                        }
                        ++cycleLength;
                    }
                    (void)cycleLength;
                }
            }
        }
    }

    std::vector<uint32_t> map;
    std::vector<uint32_t> cycles;
};


struct FftRational
{
    uint32_t numerator = 0;
    uint32_t denominator = 0;
    bool operator==(const FftRational &other) const
    {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

static FftRational GetOriginalRoot(size_t fftSize, size_t pass, size_t offset)
{
    // TODO: Write this sensibly.
    size_t groupSize = 1 << pass;          // butterfly mask
    size_t twiddleOffset = groupSize >> 1; // butterfly width
    uint32_t iWDirect = (offset-twiddleOffset) % groupSize;
    return FftRational{iWDirect, (uint32_t)twiddleOffset};

    // uint32_t iW = 0;
    // uint32_t iWInc = 1;
    // // std::complex<double> wInc = std::exp(std::complex<double>(0, Pi / twiddleOffset));


    // for (size_t j = 0; j < twiddleOffset; ++j)
    // {
    //     for (size_t k = j; k < fftSize; k += groupSize)
    //     {
    //         // auto pLeft0 = &(result[k]);
    //         // auto pRight0 = &(result[k + twiddleOffset]);

    //         // *pLeft0 = 1.0;
    //         // *pRight0 = w;
    //         size_t rightIndex = k + twiddleOffset;
    //         if (rightIndex == offset)
    //         {
    //             uint32_t iWDirect = (offset-twiddleOffset) % groupSize;
    //             (void)iWDirect;
    //             assert(iWDirect == iW);
    //             return FftRational{iW, (uint32_t)twiddleOffset};
    //         }
    //     }
    //     iW = iW + iWInc;
    // }
    // throw std::logic_error("Something went wrong.");
}



static std::vector<std::complex<double>> ComputeShuffleButterflyFactors(size_t pass, size_t fftSize)
{
    std::vector<std::complex<double>> result;
    result.resize(fftSize);

    size_t groupSize = 1 << pass;          // butterfly mask
    size_t twiddleOffset = groupSize >> 1; // butterfly width

    std::complex<double> wInc = std::exp(std::complex<double>(0, Pi / twiddleOffset));

    std::complex<double> w = 1.0;

    for (size_t j = 0; j < twiddleOffset; ++j)
    {
        for (size_t k = j; k < fftSize; k += groupSize)
        {
            auto pLeft0 = &(result[k]);
            auto pRight0 = &(result[k + twiddleOffset]);

            *pLeft0 = 1.0;
            *pRight0 = w;
        }
        w = w * wInc;
    }
    return result;
}

static InPlaceShuffle GenerateShuffle(size_t pass, size_t fftSize)
{
    std::vector<uint32_t> map;
    map.resize(fftSize);

    // For small sections, do butterflies in the most compute-efficient order.
    size_t groupSize = 1 << pass;          // butterfly mask
    size_t twiddleOffset = groupSize >> 1; // butterfly width

    // fft butterflies, 2 at a time in order to encourage use of f64x2 SIMD instructions.
    uint32_t ix = 0;
    for (size_t j = 0; j < twiddleOffset; ++j)
    {
        for (size_t k = j; k < fftSize; k += groupSize)
        {

            // auto pLeft0 = &(result[k]);
            // auto pRight0 = &(result[k + twiddleOffset]);
            map[ix++] = k;
            map[ix++] = k + twiddleOffset;
        }
    }
    return InPlaceShuffle(map);
}


struct StageNShuffleFactor
{
    std::complex<double> w0;
    std::complex<double> wInc;
    uint32_t holdCount;

    bool operator==(const StageNShuffleFactor &other) const
    {
        return w0 == other.w0 && wInc == other.wInc;
    }
};

using StageNShuffleVector = std::vector<StageNShuffleFactor>;

static void StageNShufflePass(VectorRange<complex_t> &output, const StageNShuffleVector &shuffleVector, size_t stageIndex, StagedFft::Direction dir)
{

    size_t fftSize = output.size();
    // For small sections, do butterflies in the most compute-efficient order.
    size_t groupSize = 1 << (stageIndex + 1); // butterfly mask
    size_t twiddleOffset = groupSize >> 1;    // butterfly width
    double conj = dir == StagedFft::Direction::Forward ? 1 : -1;

    // complex_t wInc = std::exp(complex_t(0, Pi / twiddleOffset * double(dir)));
    complex_t wj;
    complex_t wInc;

    for (size_t j = 0; j < twiddleOffset; j += 1)
    {
        const auto &entry = shuffleVector[j];

        complex_t wj = entry.w0;
        wj = complex_t(wj.real(), wj.imag() * conj);

        complex_t wInc = entry.wInc;
        wInc = complex_t(wInc.real(), wInc.imag() * conj);

        uint32_t holdCount = entry.holdCount;
        uint32_t exponentCount = 0;

        for (size_t k = j; k < fftSize; k += groupSize)
        {

            complex_t *RESTRICT pLeft0;
            complex_t *RESTRICT pRight0;

            pLeft0 = &(output[k]);
            pRight0 = &(output[k + twiddleOffset]);

            complex_t iLeft = *pLeft0;
            complex_t iRight = wj * (*pRight0);
            *pLeft0 = iLeft + iRight;
            *pRight0 = iLeft - iRight;
            if (++exponentCount >= holdCount)
            {
                exponentCount = 0;
                wj = wj * wInc;
            }
        }
    }
}
static StageNShuffleVector MakeStageNShuffleFactors(size_t originalFftSize, size_t originalPass, size_t stageIndex, size_t slice, size_t end, const InPlaceShuffle &shuffle)
{
    size_t stage0FftSize = end - slice;
    StageNShuffleVector result;
    size_t pass = stageIndex + 1;

    size_t groupSize = 1 << pass;          // butterfly mask
    size_t twiddleOffset = groupSize >> 1; // butterfly width

    FftRational startingRoot;
    FftRational nextRoot;
    uint32_t holdCount = 0;
    bool hasIncrement = false;
    for (size_t j = 0; j < twiddleOffset; ++j)
    {
        for (size_t k = j; k < stage0FftSize; k += groupSize)
        {
            size_t rightIndex = k + twiddleOffset;

            size_t originalRightIndex = shuffle.Map(slice + rightIndex);

            FftRational t = GetOriginalRoot(originalFftSize, originalPass, originalRightIndex);
            (void)t;
            if (k == j)
            {
                startingRoot = t;
                holdCount = 1;
                hasIncrement = false;
            }
            else if (t == startingRoot && !hasIncrement)
            {
                ++holdCount;
            }
            else if (!hasIncrement)
            {
                nextRoot = t;
                hasIncrement = true;
#if defined(NDEBUG) && !(RELWITHTDEBINFO)
                break; // we have what we need.
#else
            }
            else
            {
                size_t offset = (k - j) / groupSize;
                offset /= holdCount;
                assert(t.denominator == startingRoot.denominator);
                assert(
                    t.numerator ==
                    startingRoot.numerator + (nextRoot.numerator - startingRoot.numerator) * offset);
#endif
            }
        }
        complex_t w0 = std::exp(complex_t(0, startingRoot.numerator * Pi / startingRoot.denominator));
        complex_t wInc = std::exp(complex_t(0, (nextRoot.numerator - startingRoot.numerator) * Pi / startingRoot.denominator));

        result.push_back({w0, wInc, holdCount});
    }
    return result;
}

size_t StagedFftPlan::AddShuffleOps(size_t currentPass, size_t fftSize)
{
    this->isShuffleOptimized = true;
    // building is ridiculously inefficient. I'm sure there are more closed-form solutions. But this approach
    // has the benefit of being correct, and generates optimum compute times, if not optimum build times.
    size_t log2N = log2(fftSize);
    size_t finalPass = log2N + 1;

    if (finalPass > currentPass + l1Log2CacheSize) // currentPass is, conveniently, maximum l1 cache size.
    {
        finalPass = currentPass + l1Log2CacheSize;
    }
    InPlaceShuffle shuffle = GenerateShuffle(currentPass, fftSize);
    InPlaceShuffle inverseShuffle = shuffle.MakeInverse();

    ops.push_back(
        [shuffle](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
        {
            shuffle.ShuffleInPlace(outputs);
        });
    stageFactors.resize(0);
    for (size_t p = currentPass; p < finalPass; ++p)
    {
        auto factors = ComputeShuffleButterflyFactors(p, fftSize);
        shuffle.ShuffleInPlace(factors);
        stageFactors.push_back(std::move(factors));
    }
    // for each l1 slice, generate sub-dfts one by one.

    for (size_t slice = 0; slice < fftSize; slice += l1CacheFftSize)
    {
        size_t end = slice + l1CacheFftSize;
        size_t stageIndex = 0;
        for (size_t p = currentPass; p < finalPass; ++p)
        {
            auto stageNFactors = MakeStageNShuffleFactors(fftSize, p, stageIndex, slice, end, shuffle);
            ops.push_back(
                [slice, end, stageNFactors, stageIndex](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
                {
                    VectorRange<complex_t> outputSlice{slice, end, outputs};
                    StageNShufflePass(outputSlice, stageNFactors, stageIndex, dir);
                });

            ++stageIndex;
        }
    }

    ops.push_back([inverseShuffle](InstanceData &instanceData, VectorRange<complex_t> &outputs, Direction dir)
                  { inverseShuffle.ShuffleInPlace(outputs); });

    return finalPass;
}

