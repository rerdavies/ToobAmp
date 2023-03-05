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

#include "FftConvolution.hpp"
#include <numbers>
#include <map>
#include <mutex>
#include <iostream>

using namespace LsNumerics;


static int NextPowerOf2(size_t value)
{
    size_t result = 1;
    while (result < value)
    {
        result *= 2;
    }
    return result;
}

void FftConvolution::DelayLine::SetSize(size_t size)
{
    size = NextPowerOf2(size);
    this->size_mask = size - 1;
    this->head = 0;
    this->storage.resize(0);
    this->storage.resize(size);
}

static int log2(int x)
{
    int result = 0;

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
static int BitReverse(uint32_t x, int nb)
{
    assert(nb > 0 && nb <= 32);
    x = (x << 16) | (x >> 16);
    x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
    x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
    x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
    x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);

    return ((x >> (32 - nb)) & (0xFFFFFFFF >> (32 - nb)));
}

void FftConvolution::FftPlan::Compute(const std::vector<complex_t> &input, std::vector<complex_t> &output, FftDirection dir)
{
    assert(N != -1);
    assert(input.size() >= (size_t)N);
    assert(output.size() >= (size_t)N);

    // pre-process the input data
    for (int j = 0; j < N; ++j)
        output[j] = norm * input[bitReverse[j]];

    // fft passes
    for (int i = 1; i <= log2N; ++i)
    {
        int m = 1 << i;  // butterfly mask
        int m2 = m >> 1; // butterfly width

        complex_t wj(1, 0);

        // complex_t wInc = std::exp(complex_t(0, std::numbers::pi / m2 * double(dir)));
        complex_t &t = twiddleIncrements[i];
        complex_t wInc = complex_t(t.real(), t.imag() * (double)(dir));


        // fft butterflies
        for (int j = 0; j < m2; ++j)
        {
            complex_t w = complex_t((double)wj.real(), (double)wj.imag());
            for (int k = j; k < N; k += m)
            {
                complex_t t = w * buffer[k + m2];
                complex_t u = buffer[k];
                output[k] = u + t;
                output[k + m2] = u - t;
            }
            wj *= wInc;
        }
    }
}

void FftConvolution::FftPlan::SetSize(int size)
{

    if (this->N == size)
    {
        return;
    }
    assert((size & (size - 1)) == 0); // must be power of 2!

    this->N = size;
    bitReverse.resize(N);
    buffer.resize(N);

    log2N = log2(N);

    for (int j = 0; j < N; ++j)
    {
        bitReverse[j] = BitReverse(j, log2N);
    }
    norm = 1 / std::sqrt((double)(N));
    twiddleIncrements.resize(log2N + 1);
    for (int i = 1; i <= log2N; ++i)
    {
        int m = 1 << i;  // butterfly mask
        int m2 = m >> 1; // butterfly width

        complex_t wj(1, 0);
        complex_t wInc = std::exp(complex_t(0, std::numbers::pi / m2));
        twiddleIncrements[i] = wInc;
    }
}

std::mutex FftConvolution::FftPlan::cacheMutex;
std::map<size_t,FftConvolution::FftPlan::ptr> FftConvolution::FftPlan::planCache;

FftConvolution::FftPlan::ptr FftConvolution::FftPlan::GetCachedPlan(size_t size) {
    std::lock_guard<std::mutex> lock { cacheMutex };
    if (planCache.contains(size))
    {
        return planCache[size];
    }
    ptr plan { new FftPlan(size)};
    planCache[size] = plan;

    return plan;
}

FftConvolution::Section::Section(size_t size, size_t offset, const std::vector<float> &impulseSamples)
:size(size)
,sampleOffset(offset)
,fftPlan(FftPlan::GetCachedPlan(size*2))
{
    inputOffset = offset-size;
    impulseFft.resize(size*2);
    double impulseNorm = sqrt(double(size*2));
    for (size_t i = 0; i < impulseFft.size(); ++i)
    {
        impulseFft[i] *= impulseNorm;
    }

    buffer.resize(size*2);
    fftPlan->Compute(offset,impulseSamples,this->impulseFft,FftPlan::FftDirection::Forward);    
}

void FftConvolution::Section::Update(const DelayLine&delayLine)
{
    fftPlan->Compute(this->inputOffset,delayLine,buffer,FftPlan::FftDirection::Forward);
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        buffer[i] *= impulseFft[i];
    }
    fftPlan->Compute(this->buffer,this->buffer,FftPlan::FftDirection::Backward);
}

FftConvolution::FftConvolution(std::vector<float> &samples) {
    size_t size = samples.size();
    size_t fftSize = MINIMUM_FFT_SIZE;
    size_t fftDelay = Section::GetSectionDelay(fftSize);
    directConvolutionLength = MINIMUM_DIRECT_CONVOLUTION_LENGTH +fftDelay;

    size_t sampleIndex = directConvolutionLength;
    while (sampleIndex < size) {

        Section section(fftSize,sampleIndex,samples);
        this->sections.push_back(std::move(section));

        sampleIndex += fftSize;

        ptrdiff_t remaining = size-sampleIndex;
        if (remaining > 0)
        {
            if (((size_t)remaining) >= (fftSize))
            {
                ptrdiff_t firstAccess = sampleIndex-fftSize*2;
                if (firstAccess >= (ptrdiff_t)MINIMUM_DIRECT_CONVOLUTION_LENGTH)
                {
                    fftSize *= 2;
                    fftDelay = Section::GetSectionDelay(fftSize);
                }
            } else if (((size_t)remaining) *2 < fftSize && fftSize > MINIMUM_FFT_SIZE)
            {
                fftSize /= 2;
                fftDelay = Section::GetSectionDelay(fftSize);
            }
        }
    }
    if (directConvolutionLength > size)
    {
        directConvolutionLength = size;
    }
    directImpulse.resize(directConvolutionLength);
    for (size_t i = 0; i < directImpulse.size(); ++i)
    {
        directImpulse[i] = samples[i];
    }
    size_t delayLineSize = 2*directConvolutionLength;
    for (Section &section: sections)
    {
        size_t maxSample = section.InputOffset() + 2 * section.Size();
        if (maxSample > delayLineSize)
        {
            delayLineSize = maxSample;
        }
    }
    delayLine.SetSize(delayLineSize);


}
