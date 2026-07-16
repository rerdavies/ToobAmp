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

#include "ResamplerDecoderStream.hpp"
#include <cmath>
#include "../restrict.hpp"

using namespace toob;



ResamplerDecoderStream::ptr ResamplerDecoderStream::create(AudioDecoderStream::ptr sourceStream, uint32_t sampleRate, const LoopParameters&loopParameters)
{
    auto result = std::make_shared<ResamplerDecoderStream>();
    result->openLoop(sourceStream,sampleRate,loopParameters);
    return result;
}

static uint32_t gcd(uint32_t left, uint32_t right) 
{
    while (right != 0) {
        uint32_t t = right;
        right = left % right;
        left = t;
    }
    return left;
}

void ResamplerDecoderStream::openLoop(AudioDecoderStream::ptr sourceStream, uint32_t sampleRate, const LoopParameters&loopParameters)
{
    this->sourceStream = sourceStream;
    this->channels = sourceStream->getChannelCount();
    this->sampleRate = sampleRate;
    this->loopParameters = loopParameters;
    this->endOfFile = sourceStream->eof();


    initResampling();

    this->rateConversion = this->sampleRate/(double)sourceStream->getSampleRate();

    this->numFrames_ = (size_t)std::ceil(sourceStream->numFrames()*rateConversion);
    this->currentFrame_ = (size_t)std::floor(sourceStream->currentFrame()*rateConversion);

    if (loopParameters.loopEnable_)
{
        this->loopStart_ = (size_t)(loopParameters.loopStart_*sampleRate);
        this->loopEnd_ = (size_t)(loopParameters.loopEnd_*sampleRate);
        if (this->loopEnd_ > numFrames_) loopEnd_ = numFrames_;
        if (this->loopStart_ >= this->loopEnd_ || loopStart_ >= numFrames_) {
            this->loopStart_ = 0;
            this->loopEnd_ = std::numeric_limits<size_t>::max();
        }
    } else {
        this->loopStart_ = 0;
        this->loopEnd_ = std::numeric_limits<size_t>::max();
    }

}


int ResamplerDecoderStream::getChannelCount() const {
    return sourceStream->getChannelCount();
}

bool ResamplerDecoderStream::eof() const {
    return endOfFile;
}

size_t ResamplerDecoderStream::numFrames() const {
    return numFrames_;
}

size_t ResamplerDecoderStream::currentFrame() const {
    size_t t = currentFrame_;
    while (t >= this->loopEnd_) 
    {
        t -= loopEnd_-loopStart_;
    }
    return t;
}

void ResamplerDecoderStream::close() {
    this->sourceStream->close();
}



size_t ResamplerDecoderStream::read(float **buffers, size_t frames)
{

    size_t outputIndex = 0;
    while (outputIndex < frames) 
    {

        size_t available = this->resultBuffer.getSize()-this->resultBufferOffset;
        if (available == 0) 
        {
            if (sourceStream->eof()) 
            {
                endOfFile = true;
            }
            // Shift current frame down to the previous-frame slot for filter context
            for (size_t c = 0; c < sourceBuffer.getChannelCount(); ++c) 
            {
                float* buffer = sourceBuffer.getChannel(c).data();
                float *restrict src = buffer + this->sourceFrameSize;
                float *restrict dest = buffer;
                for (size_t i = 0; i < this->sourceFrameSize; ++i)
                {
                    dest[i] = src[i];
                }
            }
            this->currentFrame_ = (size_t)(sourceStream->currentFrame()*rateConversion);
            constexpr int MAX_CHANNELS = 8;
            float *buffers[MAX_CHANNELS+1] { nullptr};
            if (sourceBuffer.getChannelCount() > MAX_CHANNELS) 
            {
                throw std::runtime_error("File has too many channels.");
            }
            for (size_t c = 0; c < sourceBuffer.getChannelCount(); ++c) 
            {
                float *p = sourceBuffer.getChannel(c).data() + sourceFrameSize;
                buffers[c] = p;
            }
            buffers[sourceBuffer.getChannelCount()] = nullptr; // defensive marker.
            sourceStream->read(buffers,this->sourceFrameSize);

            for (size_t c = 0; c < sourceBuffer.getChannelCount(); ++c) 
            {
                resampleChannel(resultBuffer.getChannel(c).data(), sourceBuffer.getChannel(c).data());
            }
            this->resultBufferOffset = 0;
            available = resultBuffer.getSize();

        }
        size_t thisTime = std::min(available, frames-outputIndex);
        for (int c = 0; c < this->channels; ++c) 
        {
            float * restrict src = resultBuffer.getChannel(c).data() + resultBufferOffset ;
            float *restrict dest = buffers[c] + outputIndex;
            for (size_t i = 0; i <thisTime; ++i)
            {
                dest[i] = src[i];
            }
        }
        outputIndex += thisTime;
        resultBufferOffset += thisTime;
    }
    return frames;
}

uint32_t ResamplerDecoderStream::getSampleRate() const {
    return this->sampleRate;
}
void ResamplerDecoderStream::initResampling()
{
    constexpr int L = RESAMPLER_FILTER_HALF_LENGTH;
    constexpr int FILTER_LEN = 2 * L + 1;

    this->sampleRateGcd = gcd(sampleRate, sourceStream->getSampleRate());

    uint32_t rawSourceFrameSize = sourceStream->getSampleRate() / sampleRateGcd;
    uint32_t rawResultFrameSize = this->getSampleRate() / sampleRateGcd;

    // Scale up so that sourceFrameSize > L, keeping the ratio exact.
    // This handles integer-ratio conversions (e.g. 48000->96000) where rawSourceFrameSize == 1.
    uint32_t scale = (rawSourceFrameSize <= (uint32_t)L)
                   ? ((uint32_t)L / rawSourceFrameSize + 1)
                   : 1u;
    this->sourceFrameSize = rawSourceFrameSize * scale;
    this->resultFrameSize = rawResultFrameSize * scale;

    // Extend source buffer by L on the right so filter taps never go out of bounds
    sourceBuffer = AudioData(sourceStream->getSampleRate(), sourceStream->getChannelCount(),
                             sourceFrameSize * 2 + L);
    resultBuffer = AudioData(sampleRate, channels, resultFrameSize);
    resultBufferOffset = resultBuffer.getSize();

    // Cutoff frequency normalized to source sample rate: lowpass at min(source, dest) Nyquist
    const double cutoff = std::min(1.0, (double)resultFrameSize / sourceFrameSize);

    resamplingOffsets.resize(resultFrameSize);
    resamplingCoefficients.resize(resultFrameSize * FILTER_LEN);

    for (uint32_t j = 0; j < resultFrameSize; ++j)
    {
        // Position in source buffer corresponding to output sample j.
        // Current frame starts at sourceFrameSize; output sample 0 maps to that position.
        double src_pos = sourceFrameSize + (double)j * sourceFrameSize / resultFrameSize;
        int center = (int)std::floor(src_pos);
        double frac = src_pos - center; // fractional delay in [0, 1)

        resamplingOffsets[j] = center - L;

        float* coeffs = &resamplingCoefficients[j * FILTER_LEN];
        double sum = 0.0;
        for (int k = 0; k < FILTER_LEN; ++k)
        {
            // Windowed-sinc: distance from tap k to the fractional source position
            double x = cutoff * ((k - L) - frac);
            double sinc_val = (std::abs(x) < 1e-10) ? 1.0
                            : std::sin(M_PI * x) / (M_PI * x);
            // Zita-resampler window: 3-term raised cosine tuned for 60 dB stopband
            double wx = (double)std::abs(k - L) / L;
            double w = (wx >= 1.0) ? 0.0
                     : 0.384 + 0.500 * std::cos(M_PI * wx) + 0.116 * std::cos(2.0 * M_PI * wx);
            coeffs[k] = (float)(cutoff * sinc_val * w);
            sum += coeffs[k];
        }
        // Normalize to unity gain at DC
        if (std::abs(sum) > 1e-10)
        {
            float inv_sum = (float)(1.0 / sum);
            for (int k = 0; k < FILTER_LEN; ++k)
                coeffs[k] *= inv_sum;
        }
    }
}

void ResamplerDecoderStream::resampleChannel(float* dest, const float* source)
{
    constexpr int FILTER_LEN = 2 * RESAMPLER_FILTER_HALF_LENGTH + 1;
    for (uint32_t j = 0; j < resultFrameSize; ++j)
    {
        const float* restrict coeffs = &resamplingCoefficients[j * FILTER_LEN];
        const float* restrict src = source + resamplingOffsets[j];
        float sum = 0.0f;
        for (int k = 0; k < FILTER_LEN; ++k)
            sum += coeffs[k] * src[k];
        dest[j] = sum;
    }
}
