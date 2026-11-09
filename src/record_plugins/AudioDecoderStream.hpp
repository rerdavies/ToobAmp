/*
 *   Copyright (c) 2025 Robin E. R. Davies
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

#include <cstdint>
#include <filesystem>
#include <vector>

#include "LoopParameters.hpp"

namespace toob
{

    class AudioDecoderStream
    {
    protected:
        AudioDecoderStream() = default;
    public:
        AudioDecoderStream(const AudioDecoderStream&) = delete;
        AudioDecoderStream(AudioDecoderStream&&) = delete;
        AudioDecoderStream&operator=(const AudioDecoderStream&) = delete;
        AudioDecoderStream&operator=(AudioDecoderStream&&) = delete;


        using self = AudioDecoderStream;
        using ptr = std::shared_ptr<self>;

        static ptr create(const std::filesystem::path &file, int channels, uint32_t sampleRate, const LoopParameters&loopParameters);
        virtual ~AudioDecoderStream() = default;
        virtual size_t numFrames() const = 0;

        virtual size_t read(float **buffers, size_t frames) = 0;
        virtual void close() = 0;
        virtual bool eof() const = 0;
        virtual size_t currentFrame() const = 0;
        virtual uint32_t getSampleRate() const = 0;
        virtual int getChannelCount() const = 0;
    };


} // namespace toob