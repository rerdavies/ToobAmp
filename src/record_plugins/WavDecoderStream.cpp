/*
 *   Copyright (c) 2026 Robin E.R. Davies
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

#include "WavDecoderStream.hpp"
#include "../restrict.hpp"
#include "../ss.hpp"
#include <limits>

using namespace toob;


void WavAudioDecoderStream::open(const std::filesystem::path &file, int channels, uint32_t sampleRate, double seekPosSeconds)
{
    wavReader = std::make_shared<WavReader>();
    wavReader->Open(file);
    endOfFileFrame = wavReader->NumberOfFrames();
    this->endOfFile = endOfFileFrame == 0;
    readBuffer = AudioData(wavReader->SampleRate(), wavReader->Channels(),READ_BUFFER_SIZE);
    readBufferOffset = readBuffer.getSize();
    bufferFileOffset = (size_t)-1;
    endOfFileFrame = wavReader->NumberOfFrames();
    fileOffset = 0;

    start = (size_t)(sampleRate*seekPosSeconds);
    loopStart = 0;
    loopEnd = std::numeric_limits<size_t>::max();
    seek(start);
}

void WavAudioDecoderStream::seek(size_t frame)
{
    // can we seek entirely in the current buffer?
    if (bufferFileOffset != (size_t)-1 && frame >= bufferFileOffset && frame < bufferFileOffset + readBuffer.getSize())
    {
        readBufferOffset = frame-bufferFileOffset;
        fileOffset = frame;
        return;
    }
    readBufferOffset = readBuffer.getSize();
    fileOffset = frame;
    endOfFile = fileOffset >= endOfFileFrame;
    wavReader->Seek(frame);
}
void WavAudioDecoderStream::openLoop(
    const std::filesystem::path &file,
    int channels,
    uint32_t sampleRate,
    const LoopParameters&loopParameters
)
{
    this->wavReader = std::make_shared<WavReader>();
    try {
        wavReader->Open(file);
        
        endOfFileFrame = wavReader->NumberOfFrames();
        this->endOfFile = endOfFileFrame == 0;
    } catch (const std::exception&)
    {
        this->wavReader.reset();
        this->endOfFile = true;
        throw;
    }
    sampleRate = wavReader->SampleRate();

    this->sampleRate = sampleRate;
    this->channels = channels;
    this->start = (size_t)(loopParameters.start_*sampleRate);
    if (loopParameters.loopEnable_)
    {
        this->loopStart = (size_t)(loopParameters.loopStart_*sampleRate);
        this->loopEnd = (size_t)(loopParameters.loopEnd_*sampleRate);
        if (loopEnd >= endOfFileFrame)
        {
            loopEnd = endOfFileFrame;
        }
    } else {
        this->loopStart = 0;
        this->loopEnd = std::numeric_limits<size_t>::max();
    }
    if (this->start > this->loopEnd) 
    {
        this->start = this->loopStart;
    }
    if (start > endOfFileFrame) {
        start = endOfFileFrame;
    }
    if (start > loopEnd) 
    {
        start = loopStart;
    }
    if (loopStart > endOfFileFrame)
    {
        loopStart = 0;
        loopEnd = std::numeric_limits<size_t>::max();
    }

    readBuffer = AudioData(wavReader->SampleRate(), wavReader->Channels(),READ_BUFFER_SIZE);
    readBufferOffset = readBuffer.getSize();;
    fileOffset = 0;
    if (start != 0) 
    {
        seek(start);
    }

}
size_t WavAudioDecoderStream::read(float **buffers, size_t frames)
{
    size_t outputOffset = 0;
    size_t nRead = 0;
    while (frames) 
    {
        if (endOfFile) 
        {
            for (int c = 0; c < this->channels; ++c) 
            {
                float * restrict dest = buffers[c] + outputOffset;
                for (size_t i = 0; i < frames; ++i) 
                {
                    dest[i] = 0;
                }
            }
            return nRead;
        }
        if (fileOffset == loopEnd) 
        {
            seek(loopStart);
        }
        size_t available = readBuffer.getSize()-this->readBufferOffset;
        if (available == 0) 
        {
            if (fileOffset >= endOfFileFrame) 
            {
                endOfFile = true;
                continue;
            }
            bufferFileOffset = fileOffset;
            wavReader->Read(readBuffer,readBuffer.getSize());
            available = readBuffer.getSize();
            readBufferOffset = 0;
        }
        size_t thisTime = std::min(available,frames);
        if (this->fileOffset + thisTime > loopEnd) 
        {
            thisTime = this->loopEnd-this->fileOffset;

        }
        for (int outputChannel = 0; outputChannel < this->channels; ++outputChannel) 
        {
            float* restrict dest = buffers[outputChannel] + outputOffset;
            float *restrict src;
            if (outputChannel < (int)wavReader->Channels()) 
            {
                src = readBuffer.getChannel(outputChannel).data()+ readBufferOffset;
            } else if (wavReader->Channels() == 1) {
                src = readBuffer.getChannel(0).data()+ readBufferOffset;
            } else {
                throw std::runtime_error(SS("Channel conversion not supported. channels=" << this->channels << ", file channels=" << this->wavReader->Channels()));
            }
            for (size_t i = 0; i < thisTime; ++i) 
            {
                dest[i] = src[i];
            }
        }
        this->readBufferOffset += thisTime;
        outputOffset += thisTime;
        frames -= thisTime;
        this->fileOffset += thisTime;
        nRead += thisTime;
    }
    return frames;
}
void WavAudioDecoderStream::close()
{
    wavReader.reset();
    endOfFile = true;
}
bool WavAudioDecoderStream::eof() const
{
    return endOfFile;
}

size_t WavAudioDecoderStream::currentFrame() const
{
    return fileOffset;
}



WavAudioDecoderStream::ptr WavAudioDecoderStream::create(const std::filesystem::path &file, int channels, uint32_t sampleRate, double seekPosSeconds)
{
    auto result = std::make_shared<WavAudioDecoderStream>();
    result->open(file,channels,sampleRate);
    return result;
}

WavAudioDecoderStream::ptr WavAudioDecoderStream::create(const std::filesystem::path &file, int channels, uint32_t sampleRate, const LoopParameters&loopParameters)
{
    auto result = std::make_unique<WavAudioDecoderStream>();
    result->openLoop(file,channels,sampleRate,loopParameters);
    return result;
}

size_t WavAudioDecoderStream::numFrames() const {
    if (this->wavReader)
    {
        return this->wavReader->NumberOfFrames();
    }
    throw std::runtime_error("No file loaded.");
}


bool WavAudioDecoderStream::IsValidFile(const std::filesystem::path&path)
{
    return WavReader::IsWavFile(path);
}
