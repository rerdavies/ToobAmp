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
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <stdexcept>
#include "AudioData.hpp"

namespace TwoPlay
{

    class WavReaderException: public std::logic_error{
        public:
        WavReaderException(const std::string&message)
        :std::logic_error(message)
        {
            
        }
    };
    class WavReader
    {
    public:
        enum class AudioFormat {
            Invalid,
            Uint8,
            Int16,
            Int32,
            Float32,
            Float64,
        };


        void Open(const std::string &path);

        uint32_t Channels() const { return m_channels; }
        uint32_t SampleRate() const { return m_sampleRate; }
        size_t NumberOfFrames() const;
        
        void Read(AudioData&audioData);


        std::vector<std::vector<float>> ReadData();

        void ReadData(float**channels,size_t offset, size_t length);

    private:

        template<typename T>
        void ReadTypedData(float**channels,size_t offset,size_t length);

        std::vector<uint8_t> readBuffer;
        std::ifstream f;
        AudioFormat audioFormat = AudioFormat::Invalid;

        void EnterRiff();
        void ReadChunks();
        void ReadFormat();
        void ExitRiff();

        uint8_t ReadUint8() {
            char b;
            f.read(&b,1);
            if (!f)
            {
                throw std::domain_error("End of file.");
            }
            return (uint8_t)b;

        }
        int32_t ReadInt32();
        int16_t ReadInt16();
        uint32_t ReadUint32();
        uint16_t ReadUint16();


    private:
        uint32_t m_channels = 0;
        uint32_t m_sampleRate = 0;
        size_t m_frameSize = 0;

        size_t riffStart = 0;
        size_t riffEnd = 0;

        size_t dataStart = 0;
        size_t dataEnd = 0;

    };
}
