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

#include "ConvolutionReverb.hpp"
#include <complex>
#include "../ss.hpp"
using namespace LsNumerics;



class ConvolutionReverb::ConvolutionChannel
{
public:
    ConvolutionChannel(size_t frames, float *samples);

private:
    ConvolutionChannel(const ConvolutionChannel &) = delete;
    ConvolutionChannel &operator=(const ConvolutionChannel &other) = delete;
    float Tick(float input);

private:
    size_t index;
    size_t preambleSize;
    size_t fftSize;
    std::vector<float> preambleSamples;
    std::vector<float> delayLine;
};

ConvolutionReverb::ConvolutionReverb()
{
}
ConvolutionReverb::~ConvolutionReverb()
{
}

static size_t NextPowerOfTwo(size_t value)
{
    size_t result = 1;
    while (result < value)
    {
        result <<= 1;
    }
    return result;
}
void ConvolutionReverb::SetImpulseResponse(size_t frames, size_t channels, float *samples)
{
    std::vector<float> buffer(frames);
    buffer.resize(frames);

    for (size_t c = 0; c < channels; ++c)
    {
        // extract channel, reversing samples
        float *p = samples + frames * channels + c;
        for (size_t i = 0; i < frames; ++i)
        {
            p -= frames;
            buffer[i] = *p;
        }
        convolutionChannels.emplace_back(
            std::make_unique<ConvolutionChannel>(frames, &buffer.at(0)));
    }
}

ConvolutionReverb::ConvolutionChannel::ConvolutionChannel(
    size_t frames, float *samples)
    : index(0)
{
    this->preambleSize = 16;
    this->fftSize = NextPowerOfTwo(frames + preambleSize);

    size_t delayLineSize = preambleSize + fftSize;
    delayLine.resize(delayLineSize * 2);
    preambleSamples.resize(this->preambleSize);
    for (size_t i = 0; i < this->preambleSize; ++i)
    {
        preambleSamples[i] = samples[i];
    }
    std::vector<float> fftInput;
    fftInput.resize(fftSize);
    for (size_t i = preambleSize; i < frames; ++i)
    {
        fftInput[i - preambleSize] = samples[i];
    }
}

float ConvolutionReverb::ConvolutionChannel::Tick(float input)
{
    // wrapped delayline.
    size_t delayLineSize = this->fftSize + this->preambleSize;
    delayLine[this->index] = input;
    delayLine[this->index] = input;
    --index;
    if (index < delayLineSize)
    {
        index += delayLineSize;
    }
    // convolution processing.
    float result = 0;
    float *p = &(delayLine[index]);

    for (size_t i = 0; i < preambleSize; ++i)
    {
        result += *p++ * preambleSamples[i];
    }
    return result;
}
