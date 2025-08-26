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
#include "LsNumerics/LsMath.hpp"
#include <iostream>

using namespace toob;
using namespace toob::nam_impl;
using namespace LsNumerics;

static constexpr float FADE_LENGTH_SEC = 0.1;

static void bufferScale4(float *restrict buffer, float scale, size_t size)

{
    for (size_t i = 0; i < size; i += 4)
    {
        float t[4];
        t[0] = buffer[i] * scale;
        t[1] = buffer[i + 1] * scale;
        t[2] = buffer[i + 2] * scale;
        t[3] = buffer[i + 3] * scale;
        buffer[i] = t[0];
        buffer[i + 1] = t[1];
        buffer[i + 2] = t[2];
        buffer[i + 3] = t[3];
    }
}
void NamBackgroundProcessor::ThreadProc()
{
    // set RT scheduling priority (if able)

    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = 40;

    pthread_t pid = pthread_self();
    pthread_setname_np(pid, "tnam_bg");

    int result = sched_setscheduler(0, SCHED_RR, &param);
    if (result != 0)
    {
        std::cout << "ToobNAM: Failed to set background thread priority. (" << strerror(result) << ")";
    }

    uint8_t messageBuffer[1024 + 100];

    bool done = false;
    while (!done)
    {
        fgToBgQueue.read(messageBuffer, sizeof(messageBuffer), true);
        NamMessage *message = (NamMessage *)messageBuffer;
        switch (message->messageType)
        {
        case NamBgMessageType::SetDsp:
        {
            SetDspMessage *m = (SetDspMessage *)message;
            bgDsp = nullptr;
            this->bgDsp = std::unique_ptr<ToobNamDsp>(m->dsp);
            this->bgCalibrationSettings = m->calibrationSettings;
            SetBgVolumes();
            this->backgroundInputTailPosition = 0;
            this->backgroundReturnTailPosition = 0;
            this->bgInstanceId = m->instanceId;
            break;
        };
        case NamBgMessageType::SetCalibration:
        {
            SetCalibrationMessage *m = (SetCalibrationMessage *)message;
            this->bgCalibrationSettings = m->calibrationSettings;
            SetBgVolumes();
            break;
        }
        case NamBgMessageType::Quit:
        {
            bgDsp.reset();

            done = true;
            QuitMessage msg;
            this->bgToFgQueue.write(&msg, sizeof(msg));
            break;
        }
        case NamBgMessageType::SampleData:
        {
            TraceProcessing('b', 0, clock_t::duration(0));
#if TRACE_PROCESSING
            auto start = clock_t::now();
#endif
            SampleDataMessage *source = (SampleDataMessage *)message;
            size_t length = source->length;

            if (this->backgroundInputTailPosition + source->length > this->backgroundInputBuffer.size())
            {
                backgroundInputBuffer.resize(backgroundInputBuffer.size() * 2);
                backgroundReturnBuffer.resize(backgroundInputBuffer.size());
            }
            const float *restrict pIn = source->samples;
            float *restrict pOut = backgroundInputBuffer.data() + backgroundInputTailPosition;
            for (size_t i = 0; i < length; ++i)
            {
                pOut[i] = pIn[i];
            }
            backgroundInputTailPosition += length;

            size_t chunkSize = this->frameSize >= 64 ? this->frameSize : 64;
            if (backgroundInputTailPosition >= chunkSize)
            {
                // Two cases: (1) where a long frame has been assembled out of smaller pieces. i.e. Buffers are 256x3, and we've been receiving frame 128 at a time.
                // (2) THe buffer contains MORE than one frame. (e.g. Buffer size = 16, where buffers get assembed into 64 sample frames, to reduce scheduling overhead)

                // compute from input buffer to output buffer.
                size_t backgroundOutputTailPosition = 0;
                while (backgroundOutputTailPosition < backgroundInputTailPosition)
                {

                    // PROCESS NAM AUDIO FRAME
                    if (source->instanceId == this->fgInstanceId)
                    {
                        float *input = backgroundInputBuffer.data() + backgroundOutputTailPosition;
                        bufferScale4(input, bgInputVolume, frameSize);
                        float *output = backgroundReturnBuffer.data() + backgroundOutputTailPosition;

                        bgDsp->Process(
                            input, output,
                            frameSize);

                        bufferScale4(output, bgOutputVolume, frameSize);
                    }
                    else
                    {
                        // stale dsp. just return data as quickly as possible.
                        float *p = backgroundReturnBuffer.data() + backgroundOutputTailPosition;
                        for (size_t i = 0; i < frameSize; ++i)
                        {
                            p[i] = 0;
                        }
                    }
                    backgroundOutputTailPosition += this->frameSize;
                }
                for (size_t frameIndex = 0; frameIndex < backgroundOutputTailPosition; frameIndex += MAX_DATA_MESSAGE_SAMPLES)
                {
#if TRACE_PROCESSING
                    TraceProcessing('b', 1, start - clock_t::now());
#endif
                    size_t thisTime = std::min(MAX_DATA_MESSAGE_SAMPLES, backgroundOutputTailPosition - frameIndex);
                    SampleDataMessage message(bgInstanceId, thisTime);
                    pIn = backgroundReturnBuffer.data() + frameIndex;
                    pOut = message.samples;
                    for (size_t i = 0; i < thisTime; ++i)
                    {
                        pOut[i] = pIn[i];
                    }
                    bgToFgQueue.write(&message, message.MessageSize());
                }

                backgroundInputTailPosition = 0;
            }
            break;
        }
        case NamBgMessageType::StopBackgroundProcessing:
        {
            // return the currently ative ToobNamDsp to the foreground.
            StopBackgroundProcessingReplyMessage msg{this->bgDsp.release()};
            bgToFgQueue.write(&msg, sizeof(msg));
            this->bgDsp = nullptr;
            break;
        }
        case NamBgMessageType::FadeOut:
        {
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
        static_assert(offsetof(SampleDataMessage, samples) > offsetof(SampleDataMessage, length));
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
                size_t remainder = this->backgroundReturnTailPosition - nFrames;
                for (size_t i = 0; i < remainder; ++i)
                {
                    this->backgroundReturnBuffer[i] = this->backgroundReturnBuffer[i + remainder];
                }
                this->backgroundReturnTailPosition = remainder;
            }
            else
            {
                this->backgroundReturnTailPosition = 0;
            }
            return true;
        }
        if (this->backgroundQueueComplete)
        {
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
        size_t nRead = this->bgToFgQueue.read(buffer, sizeof(buffer), false);
        if (nRead == 0)
        {
            if (!wait || messageProcessed)
            {
                return messageProcessed;
            }
            this->bgToFgQueue.read(buffer, sizeof(buffer), true);
        }
        messageProcessed = true;

        NamMessage *m = (NamMessage *)buffer;
        switch (m->messageType)
        {
        case NamBgMessageType::Quit:
            this->backgroundQueueComplete = true;
            if (this->listener)
            {
                this->listener->onBackgroundProcessingComplete();
            }
            return true;
        case NamBgMessageType::SampleData:
        {
            SampleDataMessage *msg = (SampleDataMessage *)m;

            if (msg->instanceId != fgInstanceId)
            {
                return true; // silently discard if it's stale data.
            }
            size_t len = msg->length;
            if (backgroundReturnBuffer.size() < backgroundReturnTailPosition + len)
            {
                // should never happen.
                backgroundReturnBuffer.resize(backgroundReturnTailPosition + len);
            }
            float *restrict pDest = backgroundReturnBuffer.data() + backgroundReturnTailPosition;
            const float *restrict pSource = msg->samples;
            for (size_t i = 0; i < len; ++i)
            {
                pDest[i] = pSource[i];
            }
            this->backgroundReturnTailPosition += len;

            if (this->listener)
            {
                this->listener->onSamplesOut(msg->instanceId, msg->samples, msg->length);
            }
        }
        break;

        case NamBgMessageType::StopBackgroundProcessingReply:
        {
            StopBackgroundProcessingReplyMessage *msg =
                (StopBackgroundProcessingReplyMessage *)m;
            if (this->listener)
            {
                this->listener->onStopBackgroundProcessingReply(
                    msg->dsp);
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

    if (this->thread)
    {
        QuitMessage msg;
        fgToBgQueue.write(&msg, sizeof(msg));

        while (!backgroundQueueComplete)
        {
            this->fgProcessMessage(true);
        }
        this->thread.reset(); // probable  jthread join.
        // return state to pre-activate, paranoid mode.
        this->backgroundReturnTailPosition = 0;
        this->backgroundInputTailPosition = 0;
        this->threadActive = false;
    }
}


// gain access to NeuralModel protected members.
class NeuralModelHack : private NeuralAudio::NeuralModel 
{
public:

    static float GetModelOutputLevelDbu(NeuralAudio::NeuralModel*model)
    {
        return ((NeuralModelHack*)model)->modelOutputLevelDBu;
    }
    static float GetModelInputLevelDbu(NeuralAudio::NeuralModel*model)
    {
        return ((NeuralModelHack*)model)->modelInputLevelDBu;
    }

};

NamVolumeAdjustments toob::nam_impl::CalculateNamVolumeAdjustments(
    ToobNamDsp *dsp,
    const NamCalibrationSettings&calibrationSettings
)
{
    NamVolumeAdjustments result;
    if (dsp == nullptr) {
        return {0.0f,0.0f};
    }
    result.input = 1.0f; // calbration goes here later.
    if (calibrationSettings.calibrateInput)
    {
        float modelAdjustment = NeuralModelHack::GetModelInputLevelDbu(dsp);
        result.input = Db2Af(calibrationSettings.calibrationDbu - modelAdjustment);
    } else {
        result.input = 1.0f;
    }
    switch (calibrationSettings.outputCalbration)
    {
        case OutputCalibrationMode::Raw:
            result.output = 1.0;
            break;
        case OutputCalibrationMode::Normalized:
            result.output = Db2Af(dsp->GetRecommendedOutputDBAdjustment(),-200);
            break;
        case OutputCalibrationMode::Calibrated:
            result.output = Db2Af(
                NeuralModelHack::GetModelOutputLevelDbu(dsp)
                -calibrationSettings.calibrationDbu
            );
            break;
    }
    std::cout << "Calibration: in = " << Af2Db(result.input) << " out = " << Af2Db(result.output) << std::endl;
    return result;
}
void NamBackgroundProcessor::SetBgVolumes()
{
    if (bgDsp)
    {
        NamVolumeAdjustments adjustments = CalculateNamVolumeAdjustments(bgDsp.get(),bgCalibrationSettings);
        bgInputVolume = adjustments.input;
        bgOutputVolume = adjustments.output;
    }
    else
    {
        bgInputVolume = 0.0;
        bgOutputVolume = 0.0;
    }
}