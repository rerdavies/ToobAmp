/*
MIT License

Copyright (c) 2025 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <memory.h>
#include <cassert>
#include <thread>
#include <atomic>
#include "restrict.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "namFixes/dsp_ex.h"

#pragma GCC diagnostic pop

namespace toob
{
    class NeuralAmpModeler;
};
namespace toob::nam_impl
{


    class NamFadeProcessor 
    {
    public:
        NamFadeProcessor() {
            Reset();
        }
        void SetSampleRate(double sampleRate) {
            this->sampleRate = (size_t)sampleRate;
            this->maxFadeLength = (size_t)sampleRate*0.1;
            this->fadeScale = 1.0f/maxFadeLength;
        }
        void Reset()
        {
            prewarmSamples = 0;
            fadeInSamples = 0;
            fadeOutSamples = 0;
            fadedOut = false;

        }
        void Prewarm(ToobNamDsp*dsp) {
            this->prewarmSamples = (size_t)(sampleRate/4);
            this->fadeInSamples = maxFadeLength;
            this->fadeOutSamples = 0;
            this->fadedOut = false;

        }
        void FadeOut() {
            this->prewarmSamples = 0;
            this->fadeInSamples = 0;
            this->fadeOutSamples = maxFadeLength;
        }
        bool IsFadedOut() const {
            return false;
            //return fadedOut;
        }
        void Process(float*outputBuffer, size_t numFrames)
        {
            return;
            // size_t ix = 0;
            // if (prewarmSamples != 0)
            // {
            //     for (/**/; ix < numFrames && prewarmSamples != 0; ++ix,--prewarmSamples)
            //     {
            //         outputBuffer[ix] = 0;
            //     }
            // }
            // if (fadeInSamples != 0)
            // {
            //     for (/**/; ix < numFrames && fadeInSamples != 0; ++ix, --fadeInSamples) 
            //     {
            //         float vol = 1.0f-fadeInSamples*fadeScale;
            //         outputBuffer[ix] *= vol;
            //     }
            // }
            // if (fadeOutSamples != 0 && ix < numFrames)
            // {
            //     for (/**/; ix < numFrames && fadeOutSamples != 0; ++ix, --fadeOutSamples)
            //     {
            //         float vol = fadeOutSamples*fadeScale;
            //         outputBuffer[ix] *= vol;
            //     }
            //     fadedOut = fadeOutSamples == 0;
            // }

        }
    private: 
        size_t sampleRate;
        float fadeScale;
        size_t maxFadeLength;

        size_t  prewarmSamples = 0;
        size_t fadeInSamples = 0;
        size_t fadeOutSamples = 0;
        bool fadedOut = false;

    
    };

    // Packet reader/writer, blocking reads and writes. single-reader, multi-writer.
    class NamQueue
    {
    private:
        using packet_size_t = uint32_t;

    public:
        NamQueue(size_t size)
            : queue(size)
        {
        }

        void write(void *data, size_t size)
        {
            assert(size + sizeof(size_t) <= queue.size());
            std::unique_lock lock{mutex};
            while (true)
            {
                size_t available = queue.size() - sizeof(size) - count;
                if (available >= size)
                {
                    size_t tail = this->tail;
                    // write the packet length.
                    *(size_t *)(queue.data() + tail) = size;
                    tail += sizeof(size_t);

                    if (tail == queue.size())
                    {
                        tail = 0;
                    }

                    size_t firstPart = std::min(size, queue.size() - tail);
                    memcpy(queue.data() + tail, data, firstPart);
                    size_t remainder = size - firstPart;
                    if (remainder != 0)
                    {
                        memcpy(queue.data(), ((uint8_t *)data) + firstPart, remainder);
                        tail = remainder;
                    }
                    else
                    {
                        tail = tail + firstPart;
                    }
                    // round to size_t boundary.
                    tail = (tail + sizeof(size_t) - 1) & (~(sizeof(size_t) - 1));

                    assert(tail <= queue.size());
                    if (tail == queue.size())
                    {
                        tail = 0;
                    }
                    this->tail = tail;
                    this->count += size + sizeof(size_t);
                    lock.unlock();
                    cv_read.notify_one();
                    return;
                }
                cv_write.wait(lock);
            }
        }
        size_t read(uint8_t *data, size_t maxSize, bool wait)
        {
            std::unique_lock lock{mutex};
            while (true)
            {
                if (count > 0)
                {
                    size_t head = this->head;

                    // read the size.
                    size_t packetSize = *(size_t *)(queue.data() + head);
                    if (packetSize > maxSize)
                    {
                        throw std::logic_error("Packet is too large to read.");
                    }
                    head += sizeof(size_t);
                    if (head == queue.size())
                    {
                        head = 0;
                    }
                    size_t firstPart = std::min(packetSize, queue.size() - head);
                    memcpy(data, queue.data() + head, firstPart);
                    size_t remainder = packetSize - firstPart;
                    if (remainder != 0)
                    {
                        memcpy(data + firstPart, queue.data(), remainder);
                        head = remainder;
                    }
                    else
                    {
                        head += firstPart;
                    }
                    head = (head + sizeof(size_t) - 1) & (~(sizeof(size_t) - 1));

                    if (head == queue.size()) { head = 0; }
                    assert(head < queue.size());
                    this->head = head;
                    this->count -= packetSize + sizeof(size_t);
                    lock.unlock();
                    cv_write.notify_one();
                    return packetSize;
                }
                if (!wait)
                {
                    return 0;
                }
                
                while (true)
                {
                    if (cv_read.wait_for(lock,std::chrono::milliseconds(1000))
                       == std::cv_status::no_timeout)
                       break;
                }
            }
        }

    private:
        size_t head = 0;
        size_t tail = 0;
        size_t count = 0;

        std::mutex mutex;
        std::condition_variable cv_read, cv_write;

        std::vector<uint8_t> queue;
    };

    enum NamMessageType
    {
        Illegal,
        SetDsp,
        SetGain,
        SampleData,
        StopBackgroundProcessing,
        FadeOut,
        StopBackgroundProcessingReply,
        Quit,
    };
    struct NamMessage
    {
        NamMessage(NamMessageType messageType) : messageType(messageType) {}
        NamMessageType messageType;
    };
    struct SetDspMessage : public NamMessage
    {
        SetDspMessage(uint64_t instanceId, ToobNamDsp *dsp, size_t antiPopLength)
            : NamMessage(NamMessageType::SetDsp),
              instanceId(instanceId),
              dsp(dsp),
              antiPopLength(antiPopLength)
        {
        }

        ToobNamDsp *dsp;
        uint64_t instanceId;
        size_t antiPopLength;
    };
    struct StopBackgroundProcessingMessage : public NamMessage
    {
        StopBackgroundProcessingMessage() : NamMessage(NamMessageType::StopBackgroundProcessing) {}
    };
    struct FadeOutProcessingMessage : public NamMessage
    {
        FadeOutProcessingMessage() : NamMessage(NamMessageType::FadeOut) {}
    };
    // give me back my ToobNamDsp!
    struct StopBackgroundProcessingReplyMessage : public NamMessage
    {
        StopBackgroundProcessingReplyMessage(ToobNamDsp *dsp) : NamMessage(NamMessageType::StopBackgroundProcessingReply), dsp(dsp) {}
        ToobNamDsp *dsp;
    };
    static constexpr size_t MAX_DATA_MESSAGE_SAMPLES = 256;
    struct SampleDataMessage : public NamMessage
    {

        SampleDataMessage(uint64_t instanceId, size_t length)
            : NamMessage(NamMessageType::SampleData),
              instanceId(instanceId),
              length(length)
        {
        }
        SampleDataMessage(uint64_t instanceId, const float *inputData, size_t length)
            : NamMessage(NamMessageType::SampleData),
              instanceId(instanceId)

        {
            if (length > MAX_DATA_MESSAGE_SAMPLES)
            {
                throw std::logic_error("WriteSampleMessage: sample length exceeds MAX_DATA_MESSAGE_SAMPLES.");
            }
            this->length = length;
            const float *restrict p = inputData;
            for (size_t i = 0; i < length; ++i)
            {
                this->samples[i] = p[i];
            }
        }
        size_t MessageSize()
        {
            return sizeof(SampleDataMessage) - (sizeof(float) * (MAX_DATA_MESSAGE_SAMPLES - this->length));
        }
        uint64_t instanceId;
        size_t length;
        float samples[MAX_DATA_MESSAGE_SAMPLES];
    };
    struct SetGainMessage : public NamMessage
    {
        SetGainMessage(float gain)
            : NamMessage(NamMessageType::SetGain),
              gain(gain)
        {
        }

        float gain;
    };
    struct QuitMessage : public NamMessage
    {
        QuitMessage() : NamMessage(NamMessageType::Quit) {}
    };

    class NamBackgroundProcessorListener
    {
    public:
        virtual void onStopBackgroundProcessingReply(ToobNamDsp *dsp) = 0;
        virtual void onBackgroundProcessingComplete() = 0;
        virtual void onSamplesOut(uint64_t instanceId,float *data, size_t length) = 0;
    };
    class NamBackgroundProcessor
    {
        NamBackgroundProcessorListener *listener = nullptr;
        uint32_t sampleRate = 48000;
        size_t frameSize = 0;
        std::atomic<bool> backgroundQueueComplete = false;

    public:
        void SetSampleRate(double sampleRate)
        {
            this->sampleRate = (uint32_t)sampleRate;
        }
        void SetFrameSize(size_t frameSize) {
            this->frameSize = frameSize;
            this->backgroundReturnBuffer.resize(2*frameSize);
            this->backgroundInputBuffer.resize(2*frameSize);
            this->backgroundInputTailPosition = 0;
            this->backgroundReturnTailPosition = 0;
        }

        void SetListener(NamBackgroundProcessorListener *listener)
        {
            this->listener = listener;
        }
        void fgSetModel(ToobNamDsp *model, size_t antiPopSamples)
        {
            if (!thread)
            {
                threadActive = true;
                backgroundQueueComplete = false;
                
                thread = std::make_unique<std::jthread>(
                    [this]()
                    { this->ThreadProc(); });
            }
            auto instanceId = ++fgInstanceId; // xxx: pop handling.
            SetDspMessage msg{instanceId, model, antiPopSamples};
            fgToBgQueue.write(&msg, sizeof(msg));
            this->backgroundReturnTailPosition = 0; //xxx: pop handling.
            this->backgroundInputTailPosition = 0;

        }
        void fgFadeOut() 
        {
            FadeOutProcessingMessage msg;
            fgToBgQueue.write(&msg,sizeof(msg));
        }
        void fgStopBackgroundProcessing()
        {
            StopBackgroundProcessingMessage msg;
            fgToBgQueue.write(&msg, sizeof(msg));
            ++fgInstanceId; //xxx: de-pop handling.
        }
        void fgSendQuit()
        {
            if (thread)
            {
                QuitMessage msg;
                fgToBgQueue.write(&msg, sizeof(msg));
            }
        }
        void fgWrite(const float *samples, size_t nFrames);
        bool fgRead(float *samples, size_t nFrames);

        bool fgProcessMessage(bool wait);

        void fgClose();

        bool ThreadActive()
        {
            return threadActive;
        }

    private:
        void ThreadProc();
        void SetBgVolumes();



    private:
        float bgInputVolume = 0.0;
        float bgOutputVolume = 0.0;

        std::vector<float> backgroundReturnBuffer;
        size_t backgroundReturnTailPosition = 0;
        std::vector<float> backgroundInputBuffer;
        size_t backgroundInputTailPosition = 0;

        uint64_t bgInstanceId = 0;
        std::atomic<uint64_t> fgInstanceId = 0;

        std::unique_ptr<ToobNamDsp> bgDsp;
        NamQueue fgToBgQueue{8 * 1024};
        NamQueue bgToFgQueue{8 * 1024};
        std::unique_ptr<std::jthread> thread;
        std::atomic<bool> threadActive = false;
    };
}