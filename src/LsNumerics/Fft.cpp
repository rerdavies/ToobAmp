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

#include "Fft.hpp"

using namespace LsNumerics;

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

void Fft::Compute(const std::vector<std::complex<double>> &input, std::vector<std::complex<double>> &output, fft_dir dir)
{
    assert(fftSize != UNINITIALIZED_VALUE);
    assert(input.size() >= fftSize);
    assert(output.size() >= fftSize);

    size_t cnt = fftSize;
    // pre-process the input data using a borrowed buffer
    // in case input and output are aliased.
    if (&input == &output)
    {
        for (size_t j = 0; j < cnt; ++j)
        {
            windowedData[j] = norm * input[bitReverse[j]];
        }
        for (size_t j = 0; j < cnt; ++j)
        {
            output[j] = windowedData[j];
        }
    }
    else
    {
        for (size_t j = 0; j < cnt; ++j)
        {
            output[j] = norm * input[bitReverse[j]];
        }
    }
    ComputeInner(output, dir);
}
void Fft::Compute(const std::vector<float> &input, std::vector<std::complex<double>> &output, fft_dir dir)
{
    assert(fftSize != UNINITIALIZED_VALUE);
    assert(input.size() >= fftSize);
    assert(output.size() >= fftSize);

    size_t cnt = fftSize;
    // pre-process the input data
    for (size_t j = 0; j < cnt; ++j)
        output[j] = norm * input[bitReverse[j]];
    ComputeInner(output, dir);
}

void Fft::CalculateTwiddleFactors(fft_dir dir, std::vector<std::complex<double>> &twiddles)
{
    twiddles.resize(log2N+1);
    for (size_t pass = 1; pass <= log2N; ++pass)
    {
        size_t groupSize = 1 << pass;  // butterfly mask
        size_t twiddleOffset = groupSize >> 1; // butterfly width
        // fft butterflies

        twiddles[pass] = 
            std::exp(std::complex<double>(0, Pi / twiddleOffset * double(dir)));
    }
}

void Fft::ComputeInner(std::vector<std::complex<double>> &output, fft_dir dir)
{
    // fft passes
    // pass zero case. w is always 1.
    {
        constexpr size_t pass = 1;
        constexpr size_t mOffset = 1 << pass;    // butterfly mask
        constexpr size_t mStride = mOffset >> 1; // butterfly width

        // fft butterflies
        for (size_t j = 0; j < mStride; ++j)
        {
            for (size_t k = j; k < fftSize; k += mOffset)
            {
                std::complex<double> iLeft = output[k];
                std::complex<double> iRight = output[k + mStride];
                output[k] = iLeft + iRight;
                output[k + mStride] = iLeft - iRight;
            }
        }
    }
    std::vector<std::complex<double>> &twiddleFactors = (dir == fft_dir::forward) ? this->forwardTwiddle : this->backwardTwiddle;
    for (size_t pass = 2; pass <= log2N; ++pass)
    {
        size_t groupSize = 1 << pass;          // butterfly mask
        size_t twiddleOffset = groupSize >> 1; // butterfly width

        std::complex<double> wj(1, 0);
        //std::complex<double> wInc = std::exp(std::complex<double>(0, Pi / twiddleOffset * double(dir)));
        std::complex<double> wInc = twiddleFactors[pass];

        // TODO: This is the wrong order for cache efficiency!?
        // fft butterflies, 2 at a time in order to encourage use of f64x2 SIMD instructions.
        for (size_t j = 0; j < twiddleOffset; j += 2)
        {
            std::complex<double> wj2 = wj * wInc;
            for (size_t k = j; k < fftSize; k += groupSize)
            {
                std::complex<double> *RESTRICT pLeft0;
                std::complex<double> *RESTRICT pRight0;
                std::complex<double> *RESTRICT pLeft1;
                std::complex<double> *RESTRICT pRight1;

                pLeft0 = &(output[k]);
                pRight0 = &(output[k + twiddleOffset]);

                std::complex<double> iLeft = *pLeft0;
                std::complex<double> iRight = wj * (*pRight0);
                *pLeft0 = iLeft + iRight;
                *pRight0 = iLeft - iRight;

                size_t k2 = k + 1;
                pLeft1 = &output[k2];
                pRight1 = &output[k2 + twiddleOffset];
                std::complex<double> iLeft2 = output[k2];
                std::complex<double> iRight2 = wj2 * output[k2 + twiddleOffset];
                *pLeft1 = iLeft2 + iRight2;
                *pRight1 = iLeft2 - iRight2;
            }
            wj = wj2 * wInc;
        }
    }
}

void Fft::SetSize(size_t size)
{

    if (this->fftSize == size)
    {
        return;
    }
    assert((size & (size - 1)) == 0); // must be power of 2!

    this->fftSize = size;
    bitReverse.resize(fftSize);
    windowedData.resize(fftSize);

    log2N = log2(fftSize);

    for (size_t j = 0; j < fftSize; ++j)
    {
        bitReverse[j] = BitReverse(j, log2N);
    }
    norm = double(1 / std::sqrt(double(fftSize)));
    CalculateTwiddleFactors(fft_dir::forward, forwardTwiddle);
    CalculateTwiddleFactors(fft_dir::backward, backwardTwiddle);
}

// original unoptimized code.
//         void ComputeInner(std::vector<std::complex<double>> &output, fft_dir dir)
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
//                 std::complex<double> iLeft = output[k];
//                 std::complex<double> iRight = output[k + mStride];
//                 output[k] = iLeft + iRight;
//                 output[k + mStride] = iLeft - iRight;
//             }
//         }

//     }

//     for (size_t pass = 2; pass <= log2N; ++pass)
//     {
//         size_t mOffset = 1 << pass;  // butterfly mask
//         size_t mStride = mOffset >> 1; // butterfly width

//         std::complex<double> wj(1, 0);
//         std::complex<double> wInc = std::exp(std::complex<double>(0, Pi / mStride * double(dir)));

//         // fft butterflies, 2 at a time in order to encourage use of f64x2 SIMD instructions.
//         for (size_t j = 0; j < mStride; ++j)
//         {
//             std::complex<double> w = std::complex<double>((double)wj.real(), (double)wj.imag());
//             for (size_t k = j; k < fftSize; k += mOffset)
//             {
//                 std::complex<double> iLeft = output[k];
//                 std::complex<double> iRight = w * output[k + mStride];
//                 output[k] = iLeft + iRight;
//                 output[k + mStride] = iLeft - iRight;
//             }
//             wj *= wInc;
//         }
//     }
// }
