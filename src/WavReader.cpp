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

#include "WavReader.hpp"
#include <stdexcept>
#include "ss.hpp"
#include "WavGuid.hpp"

using namespace std;
using namespace TwoPlay;

#include "WavConstants.hpp"

using namespace TwoPlay::private_use;

int32_t WavReader::ReadInt32()
{
    uint8_t bytes[4];
    f.read((char *)bytes, 4);
    if (!f)
    {
        throw length_error("Unexpected end of file.");
    }
    return int32_t(
        bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[0] << 24));
}
uint32_t WavReader::ReadUint32()
{
    uint8_t bytes[4];
    f.read((char *)bytes, 4);
    if (!f)
    {
        throw length_error("Unexpected end of file.");
    }
    return uint32_t(
        bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}
int16_t WavReader::ReadInt16()
{
    uint8_t bytes[2];
    f.read((char *)bytes, 2);
    if (!f)
    {
        throw length_error("Unexpected end of file.");
    }
    return int16_t(
        bytes[0] | (bytes[1] << 8));
}
uint16_t WavReader::ReadUint16()
{
    uint8_t bytes[2];
    f.read((char *)bytes, 2);
    if (!f)
    {
        throw length_error("Unexpected end of file.");
    }
    return uint16_t(
        bytes[0] | (bytes[1] << 8));
}

void WavReader::Open(const std::string &filename)
{
    f.open(filename, ios::binary | ios::in);
    if (!f.is_open())
    {
        throw invalid_argument(SS("Can't open file. (" << filename));
    }
    EnterRiff();
    ReadChunks();

    f.seekg(this->dataStart);
}

static void ThrowFileFormatException()
{
    throw domain_error("Invalid file format.");
}
void WavReader::EnterRiff()
{
    uint32_t chunkid = ReadUint32();
    if (chunkid != (uint32_t)ChunkIds::Riff)
    {
        ThrowFileFormatException();
    }
    uint32_t chunkSize = ReadUint32();

    uint32_t riffType = ReadUint32();
    if (riffType != (uint32_t)ChunkIds::WaveRiff)
    {
        ThrowFileFormatException();
    }
    this->riffStart = f.tellg();
    this->riffEnd = this->riffStart + chunkSize;
}

void WavReader::ReadFormat()
{
    WaveFormatExtensible wf;
    wf.wFormatTag = ReadUint16();
    wf.nChannels = ReadUint16();
    wf.nSamplesPerSec = ReadUint32();
    wf.nAvgBytesPerSec = ReadUint32();
    wf.nBlockAlign = ReadUint16();
    wf.wBitsPerSample = ReadUint16();
    wf.cbSize = 0;

    if (wf.wFormatTag != (uint16_t)WavFormat::Extensible)
    {
        if (wf.wFormatTag == (uint16_t)WavFormat::PulseCodeModulation)
        {
            switch (wf.wBitsPerSample)
            { 
            case 8:
                this->audioFormat = AudioFormat::Uint8;
                break;
            case 16:
                this->audioFormat = AudioFormat::Int16;
                break;
            case 32:
                this->audioFormat = AudioFormat::Int32;
                break;
            default:
                throw domain_error("Unsupported sample format.");
            }
        }
        else
        {
            throw domain_error("Unsupported sample format.");
        }
    }
    else
    {
        wf.cbSize = ReadUint16();
        if (wf.cbSize == 22)
        {
            wf.Samples.wValidBitsPerSample = ReadUint16();
        }
        wf.dwChannelMask = ReadUint32();

        wf.SubFormat.data0 = ReadUint32();
        wf.SubFormat.data1 = ReadUint16();
        wf.SubFormat.data2 = ReadUint16();
        wf.SubFormat.data3 = ReadUint16();
        for (size_t i = 0; i < sizeof(wf.SubFormat.data4); ++i)
        {
            wf.SubFormat.data4[i] = ReadUint8();
        }

        if (wf.SubFormat == WAVE_FORMAT_PCM)
        {
            switch (wf.wBitsPerSample)
            {
            case 8:
                this->audioFormat = AudioFormat::Uint8;
                break;
            case 16:
                this->audioFormat = AudioFormat::Int16;
                break;
            case 32:
                this->audioFormat = AudioFormat::Int32;
                break;
            default:
                throw domain_error("Unsupported sample format.");
            }
        }
        else if (wf.SubFormat == WAVE_FORMAT_IEEE_FLOAT)
        {
            switch (wf.wBitsPerSample)
            {
            case 32:
                this->audioFormat = AudioFormat::Float32;
                break;
            case 64:
                this->audioFormat = AudioFormat::Float64;
                break;
            default:
                throw domain_error("Unsupported sample format.");
            }
        }
    }
    this->m_sampleRate = wf.nSamplesPerSec;
    this->m_channels = wf.nChannels;
    this->m_frameSize = wf.nBlockAlign;
}
void WavReader::ReadChunks()
{

    uint32_t chunkid = 0;
    uint32_t chunkSize = 0;
    streampos chunkStart;

    bool datachunk = false;
    while (!datachunk && f.tellg() < (ptrdiff_t)riffEnd)
    {
        chunkid = ReadUint32();
        chunkSize = ReadUint32();
        chunkStart = f.tellg();

        switch ((ChunkIds)chunkid)
        {
        case ChunkIds::Format:
            ReadFormat();
            break;
        case ChunkIds::Data:
            datachunk = true;
            dataStart = f.tellg();
            dataEnd = dataStart + chunkSize;
            return;
        default:
            break;
        }
        streampos chunkEnd = chunkStart + (std::streampos)chunkSize;
        if (chunkSize & 1)
        {
            chunkEnd = chunkEnd + (streampos)1;
        }
        f.seekg(chunkEnd);
    }
}

size_t WavReader::NumberOfFrames() const {
    return (this->dataEnd-this->dataStart)/this->m_frameSize;
}


inline float AudioInputConvert(float value) { return value; }

inline float AudioInputConvert(double value) { return (float)value; }

constexpr float CVT32 = 1.0f/(32768.0f*65536.0);


static inline float AudioInputConvert(int32_t value) { 
    return CVT32*value; 
}
constexpr float CVT16 = 1.0f/32768.0f;

static inline float AudioInputConvert(int16_t value) { 
    return CVT16*value; 
}

constexpr float CVT8 = 1.0f/(256.0);

static inline float AudioInputConvert(uint8_t value) { 
    return CVT8*value-0.5; 
}

template<typename T>
void WavReader::ReadTypedData(float**channels,size_t offset,size_t length)
{
    size_t maxLength = 64*1024/m_frameSize;
    size_t bufferSize = maxLength*this->Channels()*sizeof(T);

    if (this->readBuffer.size() < bufferSize) 
    {
        this->readBuffer.resize(bufferSize);
    }
    T*buffer = (T*)(void*)(&this->readBuffer[0]);
    while (length != 0)
    {
        size_t thisTime = std::min(maxLength,length);
        f.read((char*)(void*)buffer,thisTime*this->m_frameSize);
        if (!f)
        {
            ThrowFileFormatException();
        }
        T*p = buffer;

        for (size_t i = 0; i < thisTime; ++i)
        {
            size_t ix = offset+i;
            for (size_t chan = 0; chan < this->Channels(); ++chan)
            {
                channels[chan][ix] = AudioInputConvert(*p++);
            }
        }

        length -= thisTime;
        offset += thisTime;
    }
}



void WavReader::ReadData(float**channels,size_t offset, size_t length)
{
    switch (this->audioFormat)
    {
        case AudioFormat::Float32:
            ReadTypedData<float>(channels,offset,length);
            break;
        case AudioFormat::Float64:
            ReadTypedData<double>(channels,offset,length);
            break;
        case AudioFormat::Int16:
            ReadTypedData<int16_t>(channels,offset,length);
            break;
        case AudioFormat::Int32:
            ReadTypedData<int32_t>(channels,offset,length);
            break;
        default:
            throw WavReaderException("Unsupported format.");
    }
}

std::vector<std::vector<float>> WavReader::ReadData()
{
    std::vector<std::vector<float>> result;
    result.resize(this->m_channels);
    size_t numberOfFrames = NumberOfFrames();

    float **tResult = new float*[Channels()];

    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i].resize(numberOfFrames);
        tResult[i] = &result[i][0];
    }
    ReadData(tResult,0,numberOfFrames);

    return result;
}

void WavReader::Read(AudioData&audioData)
{
    audioData.setSampleRate(this->SampleRate());

    std::vector<std::vector<float>> data = ReadData();
    audioData.setData(std::move(data));
}
