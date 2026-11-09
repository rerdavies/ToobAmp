// Copyright (c) Robin E. R. Davies
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

#include "ToobMultiEcho.hpp"
#include "LsNumerics/Denorms.hpp"

ToobMultiEcho::ToobMultiEcho(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    : super(rate, bundle_path, features)
{
    this->sampleRate = rate;
}

ToobMultiEcho::~ToobMultiEcho()
{
}

inline void ToobMultiEcho::UpdateStereoDelays(
    ToobMultiEchoDelayUnit &leftDelay,
    ToobMultiEchoDelayUnit &rightDelay,
    RangedInputPort &delayPort,
    RangedInputPort &levelPort,
    RangedInputPort &feedbackPort,
    RangedInputPort &panPort)
{
    if (delayPort.HasChanged() || levelPort.HasChanged() || feedbackPort.HasChanged() || panPort.HasChanged())
    {
        leftDelay.Activate(ToobMultiEchoDelayUnit::Mode::Left,
                           this->sampleRate,
                           delayPort,
                           levelPort,
                           feedbackPort,
                           panPort);
        rightDelay.Activate(ToobMultiEchoDelayUnit::Mode::Right,
                            this->sampleRate,
                            delayPort,
                            levelPort,
                            feedbackPort,
                            panPort);
    }
}
inline void ToobMultiEcho::UpdateControls()
{
    bool masterHasChanged = this->master.HasChanged();
    if (masterHasChanged)
    {
        this->masterDb = this->master.GetDb();
        this->echoLevelDezipper.SetTarget(this->masterDb);
    }
    bool tailsHasChanged = this->tails.HasChanged();

    bool bypassChanged = this->bypass.HasChanged();
    this->enabled = this->bypass.GetValue() != 0;
    if (bypassChanged) 
    {
        inputLevelDezipper.SetTarget(this->enabled ? 0.0f: -96.0f);
    }
    if (tailsHasChanged || masterHasChanged || bypassChanged || this->direct.HasChanged())
    {
        this->directLevel = this->direct.GetValue()*0.01;
        if (this->enabled) {
            this->echoLevelDezipper.SetTarget(this->masterDb);
            this->directLevelDezipper.SetTarget(this->GetDirectDb());
        } else {
            this->echoLevelDezipper.SetTarget(this->tails.GetValue() != 0? this->masterDb: -96.0f);
            this->directLevelDezipper.SetTarget(0);
        }
    }
    if (this->isStereo)
    {
        UpdateStereoDelays(
            leftDelays[0], rightDelays[0],
            this->delay1,
            this->level1,
            this->feedback1,
            this->pan1);
        UpdateStereoDelays(
            leftDelays[1], rightDelays[1],
            this->delay2,
            this->level2,
            this->feedback2,
            this->pan2);
        UpdateStereoDelays(
            leftDelays[2], rightDelays[2],
            this->delay3,
            this->level3,
            this->feedback3,
            this->pan3);
        UpdateStereoDelays(
            leftDelays[3], rightDelays[3],
            this->delay4,
            this->level4,
            this->feedback4,
            this->pan4);
    }
    else
    {
        leftDelays[0].UpdateControls(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->delay1,
            this->level1,
            this->feedback1,
            this->pan1);
        leftDelays[1].UpdateControls(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->delay2,
            this->level2,
            this->feedback2,
            this->pan2);
        leftDelays[2].UpdateControls(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->delay3,
            this->level3,
            this->feedback3,
            this->pan3);
        leftDelays[3].UpdateControls(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->delay4,
            this->level4,
            this->feedback4,
            this->pan4);
    }
}
void ToobMultiEcho::Run(uint32_t n_samples)
{
    using namespace LsNumerics;

    fp_state_t savedState = disable_denorms();
    UpdateControls();
    if (isStereo)
    {
        {
            if (inputBufferL.size() < n_samples) 
            {
                inputBufferL.resize(n_samples);
            }
            if (inputBufferR.size() < n_samples)
            {
                inputBufferR.resize(n_samples);
            }
            const float * restrict inL  = this->inl.Get();
            const float * restrict inR = this->inr.Get();
            float * restrict outL = this->inputBufferL.data();
            float * restrict outR = this->inputBufferR.data();
            for (size_t i = 0; i < n_samples; ++i)
            {
                float inputLevel = this->inputLevelDezipper.Tick();
                outL[i] = inL[i]*inputLevel;
                outR[i] = inR[i]*inputLevel;
            }

        }
        float *outL = this->outl.Get();
        float *outR = this->outr.Get();
        for (size_t i = 0; i < n_samples; ++i)
        {
            outL[i] = 0;
            outR[i] = 0;
        }

        {

            const float * restrict inL  = this->inputBufferL.data();
            const float * restrict inR = this->inputBufferR.data();

            leftDelays[0].Run(n_samples, inL, outL);
            leftDelays[1].Run(n_samples, inL, outL);
            leftDelays[2].Run(n_samples, inL, outL);
            leftDelays[3].Run(n_samples, inL, outL);

            rightDelays[0].Run(n_samples, inR, outR);
            rightDelays[1].Run(n_samples, inR, outR);
            rightDelays[2].Run(n_samples, inR, outR);
            rightDelays[3].Run(n_samples, inR, outR);
        }

        for (size_t i = 0; i < n_samples; ++i)
        {
            float echoLevel = echoLevelDezipper.Tick();
            outL[i] *= echoLevel;
            outR[i] *= echoLevel;
        }
        {
            const float *inL = inl.Get();
            const float *inR = inr.Get();
            for (size_t i = 0; i < n_samples; ++i)
            {
                double directLevel = directLevelDezipper.Tick(); 
                outL[i] += inL[i] * directLevel;
                outR[i] += inR[i] * directLevel;
            }
        }

    }
    else
    {
        {
            if (inputBufferL.size() < n_samples) 
            {
                inputBufferL.resize(n_samples);
            }
            const float * restrict inL  = this->inl.Get();
            float * restrict outL = this->inputBufferL.data();
            for (size_t i = 0; i < n_samples; ++i)
            {
                float inputLevel = this->inputLevelDezipper.Tick();
                outL[i] = inL[i]*inputLevel;
            }

        }
        float *outL = this->outl.Get();
        for (size_t i = 0; i < n_samples; ++i)
        {
            outL[i] = 0;
        }

        {

            const float * restrict inL  = this->inputBufferL.data();

            leftDelays[0].Run(n_samples, inL, outL);
            leftDelays[1].Run(n_samples, inL, outL);
            leftDelays[2].Run(n_samples, inL, outL);
            leftDelays[3].Run(n_samples, inL, outL);
        }

        for (size_t i = 0; i < n_samples; ++i)
        {
            float echoLevel = echoLevelDezipper.Tick();
            outL[i] *= echoLevel;
        }
        {
            const float *inL = inl.Get();
            for (size_t i = 0; i < n_samples; ++i)
            {
                double directLevel = directLevelDezipper.Tick(); 
                outL[i] += inL[i] * directLevel;
            }
        }
    }
    restore_denorms(savedState);
}

float ToobMultiEcho::GetDirectDb() const {
    float db =  this->masterDb + AF2Db(this->directLevel);
    if (db < -96) db = -96;
    return db;

}
void ToobMultiEcho::Activate()
{

    const BufSizeOptions &bufSizeOptions = GetBuffSizeOptions();

    size_t maxBufSize = bufSizeOptions.maxBlockLength == BufSizeOptions::INVALID_VALUE ? 2048: bufSizeOptions.maxBlockLength;
    inputBufferL.resize(maxBufSize);
    inputBufferR.resize(maxBufSize);

    this->directLevelDezipper.SetSampleRate(this->sampleRate);
    this->directLevelDezipper.SetRate(0.1);
    this->echoLevelDezipper.SetSampleRate(this->sampleRate);
    this->echoLevelDezipper.SetRate(0.1);
    this->inputLevelDezipper.SetSampleRate(this->sampleRate);
    this->inputLevelDezipper.SetRate(0.1);

    this->masterDb = this->master.GetDb();
    this->directLevel = this->direct.GetValue()*0.01;
    this->enabled = this->bypass.GetValue() != 0;
    bool tails = this->tails.GetValue() != 0;

    echoLevelDezipper.Reset((enabled || tails) ? masterDb: -96.0f);
    inputLevelDezipper.Reset(this->enabled? 0.0f: -96.0f);

    if (enabled) {
        this->directLevelDezipper.Reset(GetDirectDb());
    } else {
        this->directLevelDezipper.Reset(0);
    }

    this->isStereo = this->inr.Get() != nullptr;

    if (this->isStereo)
    {
        UpdateStereoDelays(
            leftDelays[0], rightDelays[0],
            this->delay1,
            this->level1,
            this->feedback1,
            this->pan1);
        UpdateStereoDelays(
            leftDelays[1], rightDelays[1],
            this->delay2,
            this->level2,
            this->feedback2,
            this->pan2);
        UpdateStereoDelays(
            leftDelays[2], rightDelays[2],
            this->delay3,
            this->level3,
            this->feedback3,
            this->pan3);
        UpdateStereoDelays(
            leftDelays[3], rightDelays[3],
            this->delay4,
            this->level4,
            this->feedback4,
            this->pan4);
    }
    else
    {
        leftDelays[0].Activate(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->sampleRate,
            this->delay1,
            this->level1,
            this->feedback1,
            this->pan1);
        leftDelays[1].Activate(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->sampleRate,
            this->delay2,
            this->level2,
            this->feedback2,
            this->pan2);
        leftDelays[2].Activate(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->sampleRate,
            this->delay3,
            this->level3,
            this->feedback3,
            this->pan3);
        leftDelays[3].Activate(
            ToobMultiEchoDelayUnit::Mode::Mono,
            this->sampleRate,
            this->delay4,
            this->level4,
            this->feedback4,
            this->pan4);
    }
}
void ToobMultiEcho::Deactivate()
{
}

void ToobMultiEchoDelayUnit::Reset()
{
    for (size_t i = 0; i < this->delayLine.size(); ++i)
    {
        this->delayLine[i] = 0;
    }
    this->insertPosition = 0;
}

void ToobMultiEchoDelayUnit::Activate(
    Mode mode,
    double sampleRate,
    RangedInputPort &delayPort,
    RangedInputPort &levelPort,
    RangedInputPort &feedbackPort,
    RangedInputPort &panPort)
{
    this->sampleRate = sampleRate;
    size_t reservedSamples = sampleRate; // reserve 1s of audio.
    this->delayLine.reserve(reservedSamples);
    size_t delaySamples = delayPort.GetValue() * sampleRate * 0.001;
    this->tap = delaySamples;
    if (this->delayLine.size() != delaySamples)
    {
        this->delayLine.resize(delaySamples);
        this->Reset();
    }
    this->level = levelPort.GetValue() * 0.01;
    this->feedback = feedbackPort.GetValue() * 0.01;
    switch (mode)
    {
    case Mode::Mono:
        // do nothing.
        break;
    case Mode::Left:
    {
        float panValue = panPort.GetValue();
        if (panValue <= 0)
        {
            panValue = 1;
        }
        else
        {
            panValue = 1 - panValue;
        }
        this->level *= panValue;
        break;
    }
    case Mode::Right:
    {
        float panValue = panPort.GetValue();
        if (panValue >= 0)
        {
            panValue = 1;
        }
        else
        {
            panValue = 1 + panValue;
        }
        this->level *= panValue;
        break;
    }
    }
}

void ToobMultiEchoDelayUnit::UpdateControls(
    Mode mode,
    RangedInputPort &delayPort,
    RangedInputPort &levelPort,
    RangedInputPort &feedbackPort,
    RangedInputPort &panPort)
{
    if (delayPort.HasChanged() || levelPort.HasChanged() || feedbackPort.HasChanged() || panPort.HasChanged())
    {
        Activate(mode, this->sampleRate, delayPort, levelPort, feedbackPort, panPort);
    }
}

void ToobMultiEchoDelayUnit::Run(size_t nFrames, const float *restrict input, float *restrict output)
{
    while (nFrames != 0)
    {
        size_t thisTime = std::min(nFrames, this->delayLine.size() - this->insertPosition);

        float *restrict delayLine = this->delayLine.data() + this->insertPosition;
        for (size_t i = 0; i < thisTime; ++i)
        {
            float previousValue = delayLine[i];
            output[i] += previousValue;
            delayLine[i] = input[i] * this->level + previousValue * feedback;
        }
        nFrames -= thisTime;
        input += thisTime;
        output += thisTime;
        this->insertPosition += thisTime;
        if (this->insertPosition >= this->delayLine.size())
        {
            this->insertPosition = 0;
        }
    }
}
REGISTRATION_DECLARATION PluginRegistration<ToobMultiEcho> toobMultiEchoRegistration(ToobMultiEchoBase::URI);
REGISTRATION_DECLARATION PluginRegistration<ToobMultiEcho> toobMultiEchoStereoRegistration(ToobMultiEchoStereoBase::URI);
