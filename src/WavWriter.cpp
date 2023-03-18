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

#include "WavWriter.hpp"
#include <limits>
#include <cmath>

using namespace std;
using namespace TwoPlay;

#include "WavConstants.hpp"
using namespace TwoPlay::private_use;

inline size_t WavWriter::tell()
{
    auto v = f.tellp();
    return v;
}
inline void WavWriter::seek(size_t pos)
{
    f.seekp((std::streampos)pos);
}


void WavWriter::Open(const std::string &fileName)
{
    this->f.open(fileName, ios::binary | ios::out);
    if (!f)
    {
        throw invalid_argument("Can't open file " + fileName);
    }
    WriteHeader(0,0);
    EnterChunk(0);
    this->dataChunkStart = tell();
    this->isOpen = true;
}

void WavWriter::Close()
{
    if (this->isOpen)
    {
        this->isOpen = false;
        size_t dataSize = tell()-dataChunkStart;
        seek(0);
        WriteHeader(dataSize,channels);
        EnterChunk(dataSize);
        f.close();        
    }
}

static float MaxValue(const std::vector<float> &data)
{
    float max = std::numeric_limits<float>::min();

    for (float v : data)
    {
        if (abs(v) > max)
        {
            max = abs(v);
        }
    }
    return max;
}

inline void WavWriter::Write(uint8_t v)
{
    f.write((const char*)&v,1);
}
void WavWriter::Write(int32_t v)
{
    f.write((const char*)&v,sizeof(v));
}
void WavWriter::Write(int16_t v)
{
    f.write((const char*)&v,sizeof(v));
}
void WavWriter::Write(uint32_t v)
{
    f.write((const char*)&v,sizeof(v));
}
void WavWriter::Write(uint16_t v)
{
    f.write((const char*)&v,sizeof(v));
}
inline void WavWriter::WriteSample(float value)
{
    f.write((const char*)&value,sizeof(value));
}

void WavWriter::Write(uint32_t sampleRate, const std::vector<float> &data)
{
    SetSampleRate(sampleRate);
    float max = MaxValue(data);
    const float *channelData[1];
    channelData[0] = &(data[0]);

    Write(data.size(),1, channelData, 1 / (max * 4));
}
void WavWriter::Write(size_t count, size_t channels,const float **channelData, float scale)
{
    if (this->channels == 0)
    {
        this->channels = channels;
    } else {
        if (this->channels != channels)
        {   
            throw  invalid_argument("Invalid number of channels.");
        }
    }
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t c = 0; c < channels; ++c)
        {
            WriteSample(channelData[c][i]*scale);
        }
    }
}


void WavWriter::WriteHeader(size_t dataSize,size_t channels)
{
    Write((uint32_t)ChunkIds::Riff);
    Write((uint32_t)(dataSize + 8 + sizeof(WaveFormat) + 8));
    Write((uint32_t)ChunkIds::WaveRiff);

    WaveFormat wf;
    wf.wFormatTag = (uint16_t)WavFormat::IEEEFloatingPoint;
    wf.nSamplesPerSec = sampleRate;
    wf.nChannels = channels;
    wf.wBitsPerSample = sizeof(float)*8;
    wf.nBlockAlign = sizeof(float)*channels;
    wf.nAvgBytesPerSec = wf.nBlockAlign*sampleRate;


    Write((uint32_t)ChunkIds::Format);
    Write((uint32_t)sizeof(WaveFormat));

    f.write((char*)&wf,sizeof(wf));
}
void WavWriter::EnterChunk(size_t dataSize)
{
    Write((uint32_t)ChunkIds::Data);
    Write((uint32_t)dataSize);
}

