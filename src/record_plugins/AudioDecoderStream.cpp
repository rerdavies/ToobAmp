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

// thumbname:

//  ffmpeg -i 01\ Once\ in\ Royal\ David\'s\ City.mp3 -filter:v scale=-2:250 -an output.jpeg

#include "AudioDecoderStream.hpp"
#include "ResamplerDecoderStream.hpp"
#include "WavDecoderStream.hpp"
#include "AudioBufferDecoderStream.hpp"
#include "FfmpegDecoderStream.hpp"
#include "../FlacReader.hpp"

using namespace toob;

AudioDecoderStream::ptr AudioDecoderStream::create(const std::filesystem::path &file, int channels, uint32_t sampleRate, const LoopParameters &loopParameters)
{
    std::string extension = file.extension().string();
    std::shared_ptr<AudioDecoderStream> result;
    if (WavAudioDecoderStream::IsValidFile(file))
    {
        try
        {
            result = WavAudioDecoderStream::create(file, channels, sampleRate, loopParameters);
        }
        catch (const std::exception &)
        {
            throw;
            // ignored. Fall back to ffmpeg decoder.
        }
    }
    else if (FlacReader::IsFlacFile(file))
    {
        AudioData data = FlacReader::Load(file);
        result = AudioBufferDecoderStream::create(
            std::move(data),
            channels,
            loopParameters);
    }
    else
    {
        throw std::runtime_error("Invalid file.");
    }
    if (result->getSampleRate() != sampleRate)
    {
        result = ResamplerDecoderStream::create(result, sampleRate, loopParameters);
    }
    return result;
}
