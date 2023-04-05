/*
 Copyright (c) 2022 Robin Davies

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <fstream>
#include <cstddef>
#include "WavConstants.hpp"


namespace toob {
    class AudioData;

    class WavWriter {
    public:
        WavWriter() {}
        WavWriter(const std::string &fileName) { Open(fileName);}
        ~WavWriter() { Close(); }
        void Open(const std::string & fileName);
        void Close();

        void Write(uint32_t sampleRate, const std::vector<float> &data, bool normalize = false);
        void Write(const AudioData &data, bool normalize = false);

        void Write(size_t count,size_t channels, const float**data, float scale=1.0);

    private:
        void SetSampleRate(uint32_t sampleRate) {
            this->sampleRate = sampleRate;
        }

        void Write(uint8_t v);
        void Write(int32_t v);
        void Write(int16_t v);
        void Write(uint32_t v);
        void Write(uint16_t v);
        void WriteSample(float value);

        size_t tell();
        void seek(size_t size);

        void WriteHeader();
        void WriteWavFormat(size_t channels);

        void EnterRiff(private_use::ChunkIds chunkId);
        void EnterChunk(private_use::ChunkIds chunkId);
        void ExitChunk();
        void ExitRiff();

    private:
        uint32_t sampleRate = 44100;
        bool isOpen = false;
        size_t channels = 0;
        std::streamoff waveFormatStart;
        std::streamoff riffOffset;
        std::streamoff chunkOffset;
        std::ofstream f;
    };
}