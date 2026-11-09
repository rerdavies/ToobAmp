/*
 *   Copyright (c) 2026 Robin E.R. Davies
 *   All rights reserved.
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
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
#include "../WavReader.hpp"

namespace toob
{
    class WavAudioDecoderStream : public AudioDecoderStream
    {
    public:
        using super = AudioDecoderStream;
        using self = WavAudioDecoderStream;
        using ptr = std::shared_ptr<self>;

        WavAudioDecoderStream() = default;
        static bool IsValidFile(const std::filesystem::path&path);
        static ptr create(const std::filesystem::path &file, int channels, uint32_t sampleRate, double seekPosSeconds = 0.0);
        static ptr create(const std::filesystem::path &file, int channels, uint32_t sampleRate, const LoopParameters&loopParameters);

    public:
        virtual ~WavAudioDecoderStream() = default;
        virtual void open(const std::filesystem::path &file, int channels, uint32_t sampleRate, double seekPosSeconds = 0.0);
        virtual void openLoop(
            const std::filesystem::path &file,
            int channels,
            uint32_t sampleRate,
            const LoopParameters &loopParameters
            );
        virtual size_t read(float **buffers, size_t frames) override;
        virtual void close() override;
        virtual bool eof() const override;
        virtual size_t currentFrame() const override;
        virtual uint32_t getSampleRate() const override { return sampleRate; }
        virtual size_t numFrames() const override;
        virtual int getChannelCount() const override { return channels; }


    private:
        void seek(size_t frame);
        static constexpr size_t READ_BUFFER_SIZE = 65536UL;
        std::shared_ptr<WavReader> wavReader;
        AudioData readBuffer;
        size_t readBufferOffset = 0;
        size_t fileOffset = 0;
        size_t bufferFileOffset = (size_t)-1;
        size_t endOfFileFrame;
        bool endOfFile = true;

        int channels = 0;
        uint32_t sampleRate = 48000;
        size_t start = 0;
        size_t loopStart = 0;
        size_t loopEnd = 0;
    };
};


