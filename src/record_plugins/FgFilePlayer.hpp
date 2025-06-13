// Copyright (c) 2023 Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#define DEFINE_LV2_PLUGIN_BASE

#include <chrono>
#include <filesystem>
#include <memory>
#include "AudioFileBufferManager.hpp"
#include <thread>
#include "../TemporaryFile.hpp"
#include <queue>
#include "../Fifo.hpp"
#include "../json.hpp"


namespace toob
{
    class AudioFileBufferPool;
    class FfmpegDecoderStream;
};


class FgFilePlayer
{
public:
    FgFilePlayer() = default;
    ~FgFilePlayer();
    void SetBufferPool(std::shared_ptr<toob::AudioFileBufferPool> pool)
    {
        bufferPool = pool;
    }

    void OnFgCuePlaybackResponse(
        toob::AudioFileBuffer **buffers,
        size_t count,
        const LoopParameters &loopParameters,
        size_t seekPos,
        float duration);

    void OnNextBuffer(toob::AudioFileBuffer *buffer)
    {
        if (buffer)
            fgPlaybackQueue.push_back(buffer);
    }
    void OnFgError();
    void Close();

    struct FrameT
    {
        float left;
        float right;
    };
    FrameT TickStereo()
    {
#ifdef NOT_JUNK
        if (fgPlaybackQueue.empty())
        {
            return {0.0f, 0.0f};
        }
        if (fgPlaybackIndex >= sampleAvailable)
        {
            NextBuffer();
            if (fgPlaybackQueue.empty())
            {
                return {0.0f, 0.0f};
            }
        }
        FrameT result = {pLeft[fgPlaybackIndex], pRight{fgPlaybackIndex}};
        ++fgPlaybackIndex;
        return result;
#else
        return {0.0f, 0.0f};
#endif
    }
    float Tick()
    {
#ifdef NOT_JUNK
        if (fgPlaybackQueue.empty())
        {
            return 0.0f;
        }
        if (fgPlaybackIndex >= sampleAvailable)
        {
            NextBuffer();
            if (fgPlaybackQueue.empty())
            {
                return {0.0f, 0.0f};
            }
        }
        FrameT result = {pLeft[fgPlaybackIndex], pRight{fgPlaybackIndex}};
        ++fgPlaybackIndex;
        return result;
#else
        return 0.0f;
#endif
    }
    enum class PlayState
    {
        Idle = 0,
        WaitingForBuffes = 1,
        Playing = 2,
    };

private:
    void ResetPlaybackQueue()
    {
        while (!fgPlaybackQueue.empty())
        {
            bufferPool->PutBuffer(fgPlaybackQueue.pop_front());
        }
        playPosition = 0;
        fgPlaybackIndex = 0;
    }
    size_t playPosition = 0;
    size_t fgPlaybackIndex = 0;
    std::shared_ptr<toob::AudioFileBufferPool> bufferPool;
};

