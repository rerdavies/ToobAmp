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
#include "AudioData.hpp"

using namespace std;
using namespace toob;

#include "WavConstants.hpp"
using namespace toob::private_use;

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
    WriteHeader();
    this->isOpen = true;
}

void WavWriter::Close()
{
    if (this->isOpen)
    {
        this->isOpen = false;
        ExitChunk();
        ExitRiff();
        f.seekp(this->waveFormatStart);
        WriteWavFormat(this->channels);
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

void WavWriter::Write(uint32_t sampleRate, const std::vector<float> &data, bool normalize)
{
    SetSampleRate(sampleRate);
    float scale = 1.0;
    this->channels = 1;
    if (normalize)
    {
        float max = MaxValue(data);
        scale = 1/(2*max);
    }
    const float *channelData[1];
    channelData[0] = &(data[0]);

    Write(data.size(),1, channelData, scale);
}

void WavWriter::Write(const AudioData &audioData, bool normalize)
{
    SetSampleRate(audioData.getSampleRate());
    this->channels = audioData.getChannelCount();
    float scale = 1.0f;
    if (normalize)
    {
        float max = 0;
        for (size_t c = 0; c < audioData.getChannelCount(); ++c)
        {
            float t = MaxValue(audioData.getChannel(c));
            if (t > max)
            {
                max = t;
            }
        }
        scale = 1/(max*2);
    } 

    std::vector<const float*> channelPointers;
    channelPointers.reserve(audioData.getChannelCount());
    for (size_t c = 0; c < audioData.getChannelCount(); ++c)
    {
        auto&channel = audioData.getChannel(c);
        channelPointers.push_back(&(channel[0]));
    }

    Write(audioData.getSize(),audioData.getChannelCount(), &(channelPointers[0]), scale);
}

void WavWriter::Write(size_t count, size_t channels,const float **channelData, float scale)
{
    if (this->channels == 0)
    {
        this->channels = channels;
    } else {
        if (this->channels != channels)
        {   
            throw  invalid_argument("Number of channels changed.");
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


void WavWriter::EnterRiff(private_use::ChunkIds chunkId)
{
    Write((uint32_t)ChunkIds::Riff);
    Write((uint32_t)(0));
    Write((uint32_t)chunkId);
    this->riffOffset = f.tellp();
}
void WavWriter::ExitRiff()
{
    uint32_t riffSize = f.tellp()-this->riffOffset;
    f.seekp(riffOffset-2*sizeof(uint32_t));
    Write(riffSize);
}


void WavWriter::WriteHeader()
{
    EnterRiff(ChunkIds::WaveRiff);

    EnterChunk(ChunkIds::Format);
    this->waveFormatStart = this->f.tellp();

    WriteWavFormat(0);
    ExitChunk();
    EnterChunk(ChunkIds::Data);
}

void WavWriter::WriteWavFormat(size_t channels)
{

    WaveFormatExtensible wf;
    wf.wFormatTag = (uint16_t)WavFormat::Extensible;
    wf.nSamplesPerSec = sampleRate;
    wf.nChannels = channels;
    wf.wBitsPerSample = sizeof(float)*8;
    wf.nBlockAlign = sizeof(float)*channels;
    wf.nAvgBytesPerSec = wf.nBlockAlign*sampleRate;
    wf.wReserved = 0;
    wf.dwChannelMask = 0;
    wf.SubFormat = WAVE_FORMAT_IEEE_FLOAT;

    // have to write field-by-field becase Windows version densely packed (DWORD dwChannelMask is not 8-bit-aligned)
    Write(wf.wFormatTag);
    Write(wf.nChannels);
    Write(wf.nSamplesPerSec);
    Write(wf.nAvgBytesPerSec);
    Write(wf.nBlockAlign);
    Write(wf.wBitsPerSample);
    Write(wf.cbSize);
    Write(wf.wReserved);
    Write(wf.dwChannelMask);

    Write(wf.SubFormat.data0);
    Write(wf.SubFormat.data1);
    Write(wf.SubFormat.data2);
    Write((uint8_t)(wf.SubFormat.data3 >> 8));
    Write((uint8_t)(wf.SubFormat.data3));
    for (size_t i = 0; i < sizeof(wf.SubFormat.data4); ++i)
    {
        Write(wf.SubFormat.data4[i]);
    }
}
void WavWriter::EnterChunk(private_use::ChunkIds chunkId)
{
    Write((uint32_t)chunkId);
    Write((uint32_t)0);
    this->chunkOffset = f.tellp();
}

void WavWriter::ExitChunk()
{
    size_t size = f.tellp()-chunkOffset;
    if (size & 1)
    {
        Write((uint8_t)0);
    }
    auto currentPosition = f.tellp();
    f.seekp(chunkOffset-(std::streampos)(sizeof(uint32_t)));
    Write((uint32_t)size);
    f.seekp(currentPosition);
}


