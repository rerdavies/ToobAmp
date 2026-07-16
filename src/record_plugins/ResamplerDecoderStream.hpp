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

#pragma once 

#include "AudioDecoderStream.hpp"
#include "../AudioData.hpp"

namespace toob {
    class ResamplerDecoderStream: public AudioDecoderStream {
    public:
        using self = ResamplerDecoderStream;
        using super = AudioDecoderStream;
        using ptr = std::shared_ptr<self>;
        static ptr create(AudioDecoderStream::ptr sourceStream, uint32_t sampleRate, const LoopParameters&loopParameters);

        ResamplerDecoderStream() = default;
        virtual ~ResamplerDecoderStream() = default;
        virtual size_t numFrames() const override;
        
        void openLoop(
            AudioDecoderStream::ptr sourceStream, uint32_t sampleRate, const LoopParameters&loopParameters);
        virtual size_t read(float **buffers, size_t frames) override;
        virtual void close() override;
        virtual bool eof() const override;
        virtual size_t currentFrame() const override;
        virtual uint32_t getSampleRate() const override;
        virtual int getChannelCount() const override;

    private:
        static constexpr int RESAMPLER_FILTER_HALF_LENGTH = 48;

        void initResampling();
        void resampleChannel(float*result, const float*source);
        AudioData sourceBuffer;
        AudioData resultBuffer;
        size_t resultBufferOffset = 0;

        AudioDecoderStream::ptr sourceStream;
        int channels; uint32_t sampleRate;
        LoopParameters loopParameters;
        bool endOfFile = true;
        uint32_t sampleRateGcd = 0;
        uint32_t sourceFrameSize = 0;
        uint32_t resultFrameSize = 0;
        double rateConversion = 0;

        size_t numFrames_ = 0;
        size_t currentFrame_ = 0;
        size_t loopStart_ = 0;
        size_t loopEnd_ = 0;

        std::vector<int> resamplingOffsets;
        std::vector<float> resamplingCoefficients;
    };
}