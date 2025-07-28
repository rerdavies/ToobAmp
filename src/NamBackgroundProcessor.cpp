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
#include "NamBackgroundProcessor.hpp"
#include "namFixes/dsp_ex.h"
#include <iostream>

using namespace toob;
using namespace toob::nam_impl;

static constexpr float FADE_LENGTH_SEC = 0.1;

void NamBackgroundProcessor::ThreadProc()
{
    // set RT scheduling priority (if able)

    
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = 40;

    pthread_t pid = pthread_self();
    pthread_setname_np(pid,"tnam_bg");

    int result = sched_setscheduler(0, SCHED_RR, &param);
    if (result != 0)
    {
        std::cout << "ToobNAM: Failed to set background thread priority. (" << strerror(result) << ")";
    }

    uint8_t messageBuffer[1024+100];

    bool done = false;
    while (!done)
    {
        fgToBgQueue.read(messageBuffer, sizeof(messageBuffer), true);
        NamMessage *message = (NamMessage *)messageBuffer;
        switch (message->messageType)
        {
        case NamMessageType::SetDsp:
        {
            SetDspMessage *m = (SetDspMessage *)message;
            bgDsp = nullptr;
            this->bgDsp = std::unique_ptr<DSP>(m->dsp);
            this->backgroundInputTailPosition = 0;
            this->backgroundReturnTailPosition = 0;
            if (m->antiPopLength == 0)
            {
                fadeProcessor.Reset();
            } else {
                fadeProcessor.Prewarm(this->bgDsp.get());
            }
            this->bgInstanceId = m->instanceId;
            break;
        };
        case NamMessageType::SetGain:
        {
            SetGainMessage *m = (SetGainMessage *)message;
            (void)m;
            break;
        }
        case NamMessageType::Quit:
        {
            bgDsp.reset();

            done = true;
            QuitMessage msg;
            this->bgToFgQueue.write(&msg,sizeof(msg));
            break;
        }
        case NamMessageType::SampleData:
        {
            SampleDataMessage *source = (SampleDataMessage *)message;
            uint64_t instanceId = source->instanceId;
            if (instanceId != this->fgInstanceId) 
            {
                return; // data that's no longer useful. save some CPU.
            }
            size_t length = source->length;

            if (this->backgroundInputTailPosition + source->length > this->backgroundInputBuffer.size())
            {
                backgroundInputBuffer.resize(backgroundInputBuffer.size()*2);
                backgroundReturnBuffer.resize(backgroundInputBuffer.size());
            }
            const float *restrict pIn = source->samples;
            float *restrict pOut = backgroundInputBuffer.data() + backgroundInputTailPosition;
            for (size_t i = 0; i < length; ++i)
            {
                pOut[i] = pIn[i];
            }
            backgroundInputTailPosition += length;

            while (backgroundInputTailPosition >= this->frameSize) {
                // PROCESS NAM AUDIO FRAME
                if (fadeProcessor.IsFadedOut()) {
                    // free up a little CPU at an awkward and busy time!
                    float *p = backgroundReturnBuffer.data();
                    for (size_t i = 0; i < frameSize; ++i)
                    {
                        p[i] = 0;
                    }
                } else {
                    bgDsp->process(backgroundInputBuffer.data(), backgroundReturnBuffer.data(), frameSize);
                }
                fadeProcessor.Process(backgroundReturnBuffer.data(), frameSize);

                // send results back to the foreground thread.
                for (size_t ix = 0; ix < frameSize; /**/)
                {
                    size_t thisTime = std::min(MAX_DATA_MESSAGE_SAMPLES, frameSize-ix);
                    SampleDataMessage message(bgInstanceId,thisTime);
                    pIn = backgroundReturnBuffer.data()+ix;
                    pOut = message.samples;
                    for (size_t i = 0; i < thisTime; ++i)
                    {
                        pOut[i] = pIn[i];
                    }
                    bgToFgQueue.write(&message, message.MessageSize());
                    ix += thisTime;
                }


                // remove frameSize samples from the input buffer.
                if (backgroundInputTailPosition > frameSize)
                {
                    auto newTail = backgroundInputTailPosition-frameSize;
                    pIn = backgroundInputBuffer.data() + frameSize;
                    pOut = backgroundInputBuffer.data();
                    for (size_t i = 0; i < newTail; ++i) 
                    {
                        pOut[i] = pIn[i];
                    }
                    backgroundInputTailPosition = newTail;
                }   
                else {
                    backgroundInputTailPosition = 0;
                } 
            }
            break;
        }
        case NamMessageType::StopBackgroundProcessing:
        {
            // return the currently ative DSP to the foreground.
            StopBackgroundProcessingReplyMessage msg{this->bgDsp.release()};
            bgToFgQueue.write(&msg,sizeof(msg));
            this->bgDsp = nullptr;
            break;
        }
        case NamMessageType::FadeOut:
        {
            this->fadeProcessor.FadeOut();
            break;
        }

        default:
            throw std::logic_error("Invalid foreground message id.");
            break;
        }
    }

    QuitMessage quitMessage{};
    bgToFgQueue.write(&quitMessage, sizeof(quitMessage));
}

void NamBackgroundProcessor::fgWrite(const float *samples, size_t nFrames)
{
    while (nFrames)
    {
        size_t thisTime = std::min(nFrames, MAX_DATA_MESSAGE_SAMPLES);

        SampleDataMessage msg{fgInstanceId, samples, thisTime};

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Winvalid-offsetof"
        static_assert(offsetof(SampleDataMessage,samples) > offsetof(SampleDataMessage,length));
        #pragma GCC diagnostic pop
        
        fgToBgQueue.write(&msg, msg.MessageSize());
        samples += thisTime;
        nFrames -= thisTime;
    }
}

bool NamBackgroundProcessor::fgRead(float *samples, size_t nFrames)
{
    while (true)
    {
        if (backgroundReturnTailPosition >= nFrames)
        {
            for (size_t i = 0; i < nFrames; ++i)
            {
                samples[i] = this->backgroundReturnBuffer[i];
            }
            if (this->backgroundReturnTailPosition > nFrames)
            {
                // shouldn't actually ever happen.
                size_t remainder = this->backgroundReturnTailPosition-nFrames;
                for (size_t i = 0; i < remainder; ++i)
                {
                    this->backgroundReturnBuffer[i] = this->backgroundReturnBuffer[i+remainder];
                }
                this->backgroundReturnTailPosition = remainder;
            } else {
                this->backgroundReturnTailPosition = 0;
            }
            return true;
        }
        if (this->backgroundQueueComplete) {
            // an error for us to get here, really.  Shouldn't ever happen.
            for (size_t i = 0; i < nFrames; ++i)
            {
                samples[i] = 0;
            }
            return false;
        }
        fgProcessMessage(true);
    }
}


bool NamBackgroundProcessor::fgProcessMessage(bool wait)
{
    bool messageProcessed = false;
    uint8_t buffer[2048];

    while (true)
    {
        size_t nRead = this->bgToFgQueue.read(buffer,sizeof(buffer),false);
        if (nRead == 0) 
        {
            if (!wait || messageProcessed)
            {
                return messageProcessed;
            }
            this->bgToFgQueue.read(buffer,sizeof(buffer),true);
        }
        messageProcessed = true;

        NamMessage*m = (NamMessage*)buffer;
        switch (m->messageType) 
        {
            case NamMessageType::Quit:
                this->backgroundQueueComplete = true;
                if (this->listener)
                {
                    this->listener->onBackgroundProcessingComplete();
                }
                return true;
            case NamMessageType::SampleData:
            {
                SampleDataMessage*msg = (SampleDataMessage*)m;

                size_t len = msg->length;
                if (backgroundReturnBuffer.size() < backgroundReturnTailPosition+len) 
                {
                    // should never happen.
                    backgroundReturnBuffer.resize(backgroundReturnTailPosition+ len);
                }
                float* restrict  pDest = backgroundReturnBuffer.data() + backgroundReturnTailPosition;
                const float *restrict pSource  = msg->samples;
                for (size_t i = 0; i < len; ++i)
                {
                    pDest[i] = pSource[i];
                }
                this->backgroundReturnTailPosition += len;

                if (this->listener) {
                    this->listener->onSamplesOut(msg->instanceId,msg->samples,msg->length);
                }
            }
            break;

            case NamMessageType::StopBackgroundProcessingReply:
            {
                StopBackgroundProcessingReplyMessage *msg = 
                    (StopBackgroundProcessingReplyMessage *)m;
                if (this->listener)
                {
                    this->listener->onStopBackgroundProcessingReply(
                        msg->dsp
                    );
                }
                break;
            }
            default: 
                throw std::logic_error("Invalid foreground message id.");
                break;
        }
    }
}

void NamBackgroundProcessor::fgClose()
{
    // relese all resources (but be preapared for a restart)

    if (this->thread) {
        QuitMessage msg;
        fgToBgQueue.write(&msg,sizeof(msg));

        while (!backgroundQueueComplete)
        {
            this->fgProcessMessage(true);
        }
        this->thread.reset(); //probable  jthread join.
        // return state to pre-activate, paranoid mode.
        this->backgroundReturnTailPosition = 0;
        this->backgroundInputTailPosition = 0;
        this->threadActive = false;
        this->fadeProcessor.Reset();
    }
        
}