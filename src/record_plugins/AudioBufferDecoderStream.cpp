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

#include "AudioBufferDecoderStream.hpp"
#include "../restrict.hpp"
#include "../ss.hpp"
#include <limits>

using namespace toob;


void AudioBufferDecoderStream::open(AudioData&&data_, int channelCount, double seekPosSeconds)

{
    audioData = std::move(data_);
    if (audioData.getChannelCount() > channelCount)
    {
        audioData.setChannelCount(channelCount);
    }
    this->sampleRate = audioData.getSampleRate();
    this->channels = audioData.getChannelCount();
    endOfFileFrame = audioData.getSize();
    this->endOfFile = endOfFileFrame == 0;
    
    fileOffset = 0;

    start = (size_t)(sampleRate*seekPosSeconds);
    loopStart = 0;
    loopEnd = std::numeric_limits<size_t>::max();
    seek(start);
}

void AudioBufferDecoderStream::seek(size_t frame)
{
    this->fileOffset = frame;
}
void AudioBufferDecoderStream::openLoop(
    AudioData&&data_,
    int channels,
    const LoopParameters&loopParameters
)
{
    this->channels = channels;
    open(std::move(data_),channels,0);

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
    fileOffset = 0;
    if (start != 0) 
    {
        seek(start);
    }

}
size_t AudioBufferDecoderStream::read(float **buffers, size_t frames)
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
        size_t available = (fileOffset >= audioData.getSize())? 0 : (audioData.getSize()-fileOffset);
        if (available == 0)
        {
            endOfFile = true;
            continue;
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
            if (outputChannel < audioData.getChannelCount()) 
            {
                src = audioData.getChannel(outputChannel).data()+ fileOffset;
            } else if (audioData.getChannelCount() == 1) {
                //allow mono-to-stereo conversion (only)
                src = audioData.getChannel(0).data()+ fileOffset;
            } else {
                throw std::runtime_error(SS("Channel conversion not supported. channels=" << this->channels 
                    << ", file channels=" << this->audioData.getChannelCount()));
            }
            for (size_t i = 0; i < thisTime; ++i) 
            {
                dest[i] = src[i];
            }
        }
        outputOffset += thisTime;
        frames -= thisTime;
        this->fileOffset += thisTime;
        nRead += thisTime;
    }
    return frames;
}
void AudioBufferDecoderStream::close()
{
    audioData = AudioData();
    endOfFile = true;
}
bool AudioBufferDecoderStream::eof() const
{
    return endOfFile;
}

size_t AudioBufferDecoderStream::currentFrame() const
{
    return fileOffset;
}



AudioBufferDecoderStream::ptr AudioBufferDecoderStream::create(AudioData &&data, int channels, double seekPosSeconds)
{
    auto result = std::make_shared<AudioBufferDecoderStream>();
    result->open(std::move(data), channels,seekPosSeconds);
    return result;
}

AudioBufferDecoderStream::ptr AudioBufferDecoderStream::create(AudioData&&data,  int channels, const LoopParameters&loopParameters)
{
    auto result = std::make_unique<AudioBufferDecoderStream>();
    result->openLoop(std::move(data), channels,loopParameters);
    return result;
}

size_t AudioBufferDecoderStream::numFrames() const {
    return audioData.getSize();
}
