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

#include "ToobLoopers.hpp"
#include <stdexcept>
#include <numbers>
#include <cmath>
#include <ctime>
#include <string>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <iostream>
#include "FfmpegDecoderStream.hpp"

// using namespace lv2c::lv2_plugin;

using namespace toob;

static constexpr float TRANSITION_TIME_SEC = 0.003f;
static constexpr float TRIGGER_LEAD_TIME = 0.001f;
static constexpr float TRIGGER_FADE_IN_TIME = 0.001f;


static REGISTRATION_DECLARATION PluginRegistration<ToobLooperFour> registration(ToobLooperFour::URI);

constexpr char PREFERRED_PATH_SEPARATOR = std::filesystem::path::preferred_separator;

namespace toob_looper_commands
{
    enum class MessageType
    {
        RefreshPool,
        BackgroundError,
        FreeBuffer,
        Quit,
        Finished
    };

    struct BufferCommand
    {
        BufferCommand(MessageType command, size_t size) : size(size), command(command)
        {
        }
        size_t size;
        MessageType command;
    };

    struct BackgroundErrorCommmand : public BufferCommand
    {
        BackgroundErrorCommmand(const std::string &message) : BufferCommand(MessageType::BackgroundError, sizeof(BackgroundErrorCommmand))
        {
            if (message.length() > 1023)
            {
                throw std::runtime_error("Message too long.");
            }
            std::strncpy(this->message, message.c_str(), sizeof(message));
            this->size = sizeof(BackgroundErrorCommmand) + message.length() - sizeof(message) + 1;
            this->size = (this->size + 3) & (~3);
        }

        char message[1024];
    };

    struct FreeBufferCommand : public BufferCommand
    {
        FreeBufferCommand(toob::AudioFileBuffer *buffer)
            : BufferCommand(MessageType::FreeBuffer, sizeof(FreeBufferCommand)), buffer(buffer)
        {
        }
        toob::AudioFileBuffer *buffer;
    };

    struct QuitCommand : public BufferCommand
    {
        QuitCommand() : BufferCommand(MessageType::Quit, sizeof(QuitCommand))
        {
        }
    };
    struct FinishedCommand : public BufferCommand
    {
        FinishedCommand() : BufferCommand(MessageType::Finished, sizeof(QuitCommand))
        {
        }
    };

    struct ToobRefreshPoolCmmand : public BufferCommand
    {
        ToobRefreshPoolCmmand() : BufferCommand(MessageType::RefreshPool, sizeof(ToobRefreshPoolCmmand))
        {
        }
    };

}

using namespace toob_looper_commands;

ToobLooperEngine::ToobLooperEngine(int channels, double rate)
{
    this->sampleRate = rate;
    this->bufferPool = std::make_unique<toob::AudioFileBufferPool>(channels, (size_t)rate / 10);
    bufferPool->Reserve(20);
    inputTrigger.Init(rate);
    this->trigger_lead_samples = (size_t)(rate * TRIGGER_LEAD_TIME);
    leftInputDelay.SetMaxDelay(trigger_lead_samples+2048);
    rightInputDelay.SetMaxDelay(trigger_lead_samples+2048);
}


ToobLooperEngine::~ToobLooperEngine()
{
    for (auto &loop : loops)
    {
        loop.Reset();
    }
    bufferPool->Trim(0);   
}



ToobLooperFour::ToobLooperFour(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features,
    int channels)
    : super(rate, bundle_path, features),
      ToobLooperEngine(2, rate)
{

    loops.resize(N_LOOPS);

    this->isStereo = channels > 1;

    for (auto &loop : this->loops)
    {
        loop.Init(this);
    }
    loops[0].isMasterLoop = true;

    for (size_t i = 0; i < N_LOOPS; ++i)
    {
        loops[i].plugin = this;
        loops[i].sampleRate = rate;
    }
}
ToobLooperOne::ToobLooperOne(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features,
    int channels)
    : super(rate, bundle_path, features),
      ToobLooperEngine(2, rate)
{

    ///this->GetTheme() "#8750C4"
    loops.reserve(16);
    loops.resize(16);
    for (auto &loop : loops)
    {
        loop.Init(this);
    }
    loops[0].isMasterLoop = true;
    activeLoops = 1;

    this->isStereo = channels > 1;
}

void ToobLooperOne::PushLoop()
{
    size_t ix = activeLoops++;
    if (ix >= loops.size())
    {
        loops.resize(ix + 1);
        loops[ix].Init(this);
        loops[ix].master_loop_length = loops[0].master_loop_length;
    }
}

void ToobLooperOne::PopLoop()
{
    loops[activeLoops - 1].Stop(this, loops[0].play_cursor);
    if (activeLoops == 1)
    {
        this->pluginState = PluginState::Empty;
        return;
    }
    --activeLoops;
    this->pluginState = PluginState::Playing;
}

void ToobLooperFour::Activate()
{
    super::Activate();

    this->activated = true;
    this->finished = false;

    this->bufferPool->Reserve(10);

    this->backgroundThread = std::make_unique<std::jthread>(
        [this]()
        {
        try {
        bool quit = false;

        std::vector<uint8_t> buffer (2048);
        BufferCommand*cmd = (BufferCommand*)buffer.data();
        while (!quit)
        {
            this->toBackgroundQueue.readWait();
            size_t size = this->toBackgroundQueue.peekSize();
            if (size == 0) continue;

            if (size > buffer.size()) {
                buffer.resize(size);
                cmd = (BufferCommand*)buffer.data();
            }
            if (!this->toBackgroundQueue.read_packet(buffer.size()  ,(uint8_t*)cmd))
            {
                break;
            }

            try {
                switch (cmd->command) {
                case MessageType::RefreshPool:
                {
                    bufferPool->Reserve(10);
                    break;
                }
                case MessageType::FreeBuffer:
                {
                    FreeBufferCommand *freeBuffer = (FreeBufferCommand*)cmd;
                    auto buffer = freeBuffer->buffer;
                    // must zero the buffer before returning it.
                    for (size_t c = 0; c < buffer->GetChannelCount(); ++c) {
                        float *p = buffer->GetChannel(c);
                        for (size_t i = 0; i < buffer->GetBufferSize(); ++i) {
                            p[i] = 0.0f;
                        }
                    }
                    bufferPool->PutBuffer(freeBuffer->buffer);
                }
                break;

                case MessageType::Quit:
                {
                    quit = true;
                    break;
                }   
                default:
                    throw std::runtime_error("Unknown Background command.");
            }
        } catch (const std::exception&e)
        {
            std::stringstream ss;
            ss << "Background thread error: " << e.what();
            LogError(ss.str());
            BackgroundErrorCommmand errorCmd(ss.str());
            this->fromBackgroundQueue.write_packet(sizeof(errorCmd), (uint8_t*)&errorCmd);
    
        }
    }
    } catch (std::exception &e) {
        std::stringstream ss;
        ss << "Background thread error: " << e.what();
        LogError(ss.str());
        BackgroundErrorCommmand errorCmd(ss.str());
        this->fromBackgroundQueue.write_packet(sizeof(errorCmd), (uint8_t*)&errorCmd);
    }

    FinishedCommand finishedCommand;
    this->fromBackgroundQueue.write_packet(sizeof(FinishedCommand), (uint8_t*)&finishedCommand); });
}

namespace
{
    static size_t quarterNotesPerBar(TimeSig timeSig)
    {
        switch (timeSig)
        {
        case TimeSig::TwoTwo:
            return 4;
        case TimeSig::ThreeFour:
            return 3;
        case TimeSig::FourFour:
            return 4;
        case TimeSig::FiveFour:
            return 5;
        case TimeSig::SixEight:
            return 3;
        case TimeSig::SevenFour:
            return 7;
        }
        throw std::runtime_error("Unknown time sig.");
    }
    static size_t beatsPerBar(TimeSig timeSig)
    {
        switch (timeSig)
        {
        case TimeSig::TwoTwo:
            return 2;
        case TimeSig::ThreeFour:
            return 3;
        case TimeSig::FourFour:
            return 4;
        case TimeSig::FiveFour:
            return 5;
        case TimeSig::SixEight:
            return 6;
        case TimeSig::SevenFour:
            return 7;
        }
        throw std::runtime_error("Unknown time sig.");
    }
}

size_t ToobLooperEngine::GetSamplesPerQuarterNote()
{
    return (size_t)(60.0 * sampleRate / this->getTempo());
}

size_t ToobLooperEngine::GetSamplesPerBeat()
{
    size_t t = GetSamplesPerQuarterNote();
    switch (getTimesig())
    {
    case TimeSig::TwoTwo:
        t *= 2;
        break;
    case TimeSig::SixEight:
        t /= 2;
        break;
    default:
        break;
    }
    return t;
}

size_t ToobLooperEngine::GetCountInQuarterNotes()
{
    switch (getTimesig())
    {
    case TimeSig::TwoTwo:
        return 8;
    case TimeSig::ThreeFour:
        return 9;
    case TimeSig::FourFour:
        return 8;
    case TimeSig::FiveFour:
        return 5;
    case TimeSig::SixEight:
        return 6;
    case TimeSig::SevenFour:
        return 7;
    }
    throw std::runtime_error("Unknown time sig.");
}

bool ToobLooperEngine::GetCountInBlink(Loop &loop)
{
    size_t samplesPerBeat = GetSamplesPerBeat();

    size_t beat = (current_plugin_sample - loop.cue_start) / samplesPerBeat;
    size_t phase = (current_plugin_sample - loop.cue_start) % samplesPerBeat;
    bool slowBlink = phase < samplesPerBeat / 2;
    if (!slowBlink)
        return false;

    switch (getTimesig())
    {
    case TimeSig::TwoTwo:
        return true;
    case TimeSig::ThreeFour:
        // 1 .. 1 .. 1 2 3
        return beat == 0 || beat == 3 || beat >= 6;
    default:
    case TimeSig::FourFour:
        // 1 . 2 . 1 2 3 4
        return beat == 0 || beat == 2 || beat >= 4;
    case TimeSig::FiveFour:
        return true;
    case TimeSig::SixEight:
        // 1 . . 2 . . 1 2 3 4 5 6
        return beat == 0 || beat == 3 || beat >= 6;
    case TimeSig::SevenFour:
        return true;
    }
}

void ToobLooperEngine::UpdateLoopPosition(Loop & loop,RateLimitedOutputPort &progress,size_t n_frames)
{
    float pos = 0.0f;
    if (loop.isMasterLoop)
    {
        if (loop.state == LoopState::TriggerRecording)
        {
            pos = 0.0f;
        }
        else if (loop.state == LoopState::CueRecording)
        {
            size_t p = this->current_plugin_sample - loop.cue_start;
            size_t length = GetSamplesPerQuarterNote() * GetCountInQuarterNotes();
            pos = p / (float)length;
        }
        else if (loop.state == LoopState::Recording && !IsFixedLengthLoop())
        {
            // we don't have a loop lenght yet.
            size_t syncLength = (60.0f * sampleRate / getTempo()) * quarterNotesPerBar(getTimesig()) * 4;
            size_t playCursor = loop.play_cursor;
            if (syncLength == 0)
            {
                pos = 0;
            }
            else
            {
                while (playCursor > syncLength)
                {
                    syncLength *= 2;
                }
                pos = playCursor * 1.0f / syncLength;
            }
        }
        else
        {
            pos = (loop.length == 0 ? 0.0f : (float)loop.play_cursor * 1.0f / loop.length);
        }
    }
    else
    {
        if (loop.state == LoopState::CueRecording
            || loop.state == LoopState::TriggerRecording
        )
        {
            pos = (loops[0].length == 0 ? 0.0f : (float)loops[0].play_cursor * 1.0f / loops[0].length);
        }
        else
        {
            if (loop.length == 0)
            {
                pos = (0);
            }
            else
            {
                pos = (loop.length == 0 ? 0.0f : (float)loop.play_cursor * 1.0f / loop.length);
            }
        }
    }
    progress.SetValue(pos, n_frames);

}
void ToobLooperEngine::UpdateLoopLeds(
    Loop &loop,
    RateLimitedOutputPort &record_led,
    RateLimitedOutputPort &play_led)
{
    size_t currentSample = this->current_plugin_sample - this->time_zero;
    size_t slowBlinkRate = GetSamplesPerBeat();
    bool slowBlink = currentSample % slowBlinkRate < (slowBlinkRate / 2);


    switch (loop.state)
    {
    case LoopState::Idle:
        record_led.SetValue(false);
        play_led.SetValue(false);
        break;
    case LoopState::Silent:
        record_led.SetValue(false);
        play_led.SetValue(false);
        break;
    case LoopState::Recording:
        record_led.SetValue(true);
        play_led.SetValue(false);
        break;
    case LoopState::Overdubbing:
        record_led.SetValue(true);
        play_led.SetValue(true);
        break;
    case LoopState::Playing:
        record_led.SetValue(false);
        play_led.SetValue(true);
        break;
    case LoopState::CueOverdub:
        record_led.SetValue(slowBlink);
        play_led.SetValue(true);
        break;

    case LoopState::TriggerRecording:
        record_led.SetValue(slowBlink);
        play_led.SetValue(false);
        break;
    case LoopState::CueRecording:
        if (loop.isMasterLoop)
        {
            record_led.SetValue(GetCountInBlink(loop));
            play_led.SetValue(false);
        }
        else
        {
            record_led.SetValue(slowBlink);
            play_led.SetValue(false);
        }
        break;
    case LoopState::Stopping:
        record_led.SetValue(false);
        play_led.SetValue(false);
        break;
    }

    if (loop.recordError.HasError())
    {
        record_led.SetValue(loop.recordError.ErrorBlinkState());
    }
    if (loop.playError.HasError())
    {
        play_led.SetValue(loop.playError.ErrorBlinkState());
    }
}

void ToobLooperOne::UpdateLoopPosition(
    Loop & loop,
    RateLimitedOutputPort &position, 
    size_t n_frames) 
{
    ToobLooperEngine::UpdateLoopPosition(
        loop,
        position,
        n_frames);  
}

void ToobLooperOne::UpdateLoopLeds(
    ToobLooperOne::Loop &loop,
    RateLimitedOutputPort &record_led,
    RateLimitedOutputPort &play_led
    )
{
    
    ToobLooperEngine::UpdateLoopLeds(
        loop,
        record_led,
        play_led);

    if (loop.state == LoopState::CueOverdub)
    {
        SetBeatLeds(record_led, play_led);
    } 
    else if (loop.state == LoopState::TriggerRecording) {
        SetSlowBlinkLed(record_led);
        play_led.SetValue(false);
    }
    else if (loop.state == LoopState::CueRecording && !loop.isMasterLoop)
    {
        SetBeatLeds(record_led, play_led);
    }
    else if (loop.state == LoopState::Recording && !loop.isMasterLoop)
    {
        record_led.SetValue(true);
        play_led.SetValue(true);
    }
    else if (loop.state == LoopState::Playing && !loop.isMasterLoop)
    {
        record_led.SetValue(false);
        play_led.SetValue(true);
    }
}


void ToobLooperEngine::SetSlowBlinkLed(RateLimitedOutputPort &bar_led)
{
    int64_t currentSample;
    currentSample = this->current_plugin_sample - this->time_zero;
    size_t slowBlinkRate = GetSamplesPerBeat();
    bool slowBlink = currentSample % slowBlinkRate < (slowBlinkRate / 2);
    bar_led.SetValue(slowBlink);
}


void ToobLooperEngine::SetBeatLeds(RateLimitedOutputPort &bar_led, RateLimitedOutputPort &beat_led)
{
    int64_t currentSample;
    currentSample = this->current_plugin_sample - this->time_zero;

    size_t slowBlinkRate = GetSamplesPerBeat();
    size_t beatsPerBar_ = beatsPerBar(getTimesig());
    size_t beat = (currentSample / slowBlinkRate) % beatsPerBar_;
    bool slowBlink = currentSample % slowBlinkRate < (slowBlinkRate / 2);

    bar_led.SetValue((beat < beatsPerBar_ - 1));
    beat_led.SetValue(slowBlink);
}

void ToobLooperOne::UpdateOutputControls(uint64_t samplesInFrame)
{
    SetBeatLeds(bar_led, beat_led);
    UpdateLoopPosition(loops[activeLoops-1], this->position, samplesInFrame);


    if (controlDown) {
        return;
    }
    UpdateLoopLeds(loops[activeLoops - 1], this->record_led, this->play_led);

    if (loops[0].state == LoopState::Idle)
    {
        this->loop_level.SetValue(0);
    }
    else
    {
        this->loop_level.SetValue(activeLoops);
    }
}

void ToobLooperFour::UpdateOutputControls(size_t samplesInFrame)
{

    SetBeatLeds(bar_led, beat_led);


    UpdateLoopLeds(loops[0], this->record_led1, this->play_led1);
    UpdateLoopLeds(loops[1], this->record_led2, this->play_led2);
    UpdateLoopLeds(loops[2], this->record_led3, this->play_led3);
    UpdateLoopLeds(loops[3], this->record_led4, this->play_led4);

    UpdateLoopPosition(loops[0], this->position1, samplesInFrame);
    UpdateLoopPosition(loops[1], this->position2, samplesInFrame);
    UpdateLoopPosition(loops[2], this->position3, samplesInFrame);
    UpdateLoopPosition(loops[3], this->position4, samplesInFrame);


}

void ToobLooperFour::HandleTriggers()
{
    try
    {

        if (this->stop1.IsTriggered())
        {
            // clear ALL loops.
            for (size_t i = 0; i < loops.size(); ++i)
            {
                loops[i].Stop(this, 0);
            }
            has_time_zero = false;
            return;
        }
        uint64_t loop_offset = 0;
        if (has_time_zero && loops[0].length != 0)
        {
            loop_offset = (current_plugin_sample - time_zero) % loops[0].length;
        }

        if (this->record1.IsTriggered())
        {
            loops[0].Record(this, loop_offset);
        }

        if (this->play1.IsTriggered())
        {
            loops[0].Play(this, loop_offset);
        }

        if (stop2.IsTriggered())
        {
            loops[1].Stop(this, loop_offset);
        }
        if (stop3.IsTriggered())
        {
            loops[2].Stop(this, loop_offset);
        }
        if (stop4.IsTriggered())
        {
            loops[3].Stop(this, loop_offset);
        }

        loops[0].ControlValue(control1.GetValue());
        loops[1].ControlValue(control2.GetValue());
        loops[2].ControlValue(control3.GetValue());
        loops[3].ControlValue(control4.GetValue());


        // recalculate just in case the length has changed.
        loop_offset = (current_plugin_sample - time_zero) % loops[0].length;

        if (this->record2.IsTriggered())
        {
            loops[1].Record(this, loop_offset);
        }

        if (this->play2.IsTriggered())
        {
            loops[1].Play(this, loop_offset);
        }
        if (this->record3.IsTriggered())
        {
            loops[2].Record(this, loop_offset);
        }

        if (this->play3.IsTriggered())
        {
            loops[2].Play(this, loop_offset);
        }

        if (this->record4.IsTriggered())
        {
            loops[3].Record(this, loop_offset);
        }

        if (this->play4.IsTriggered())
        {
            loops[3].Play(this, loop_offset);
        }
    }
    catch (const std::exception &e)
    {
        LogError(e.what());
    }
}

void ToobLooperFour::Run(uint32_t n_samples)
{
    const float *in = this->in.Get();
    const float *inR = this->inR.Get();


    inputTrigger.ThresholdDb(trigger_level.GetDb());
    ProcessInputTrigger(in, inR, n_samples);
    inputTrigger.Run(in, inR, n_samples);
    trigger_led.SetValue(inputTrigger.TriggerLed(), n_samples);

    fgHandleMessages();

    HandleTriggers();

    Mix(
        n_samples,
        in,
        inR,
        out.Get(),
        outR.Get());

    this->current_plugin_sample += n_samples;

    UpdateOutputControls(n_samples);
}

void ToobLooperOne::Run(uint32_t n_samples)
{

    inputTrigger.ThresholdDb(trigger_level.GetDb());
    inputTrigger.Run(in.Get(), inR.Get(), n_samples);
    trigger_led.SetValue(inputTrigger.TriggerLed(), n_samples);


    fgHandleMessages();

    HandleTriggers();

    Mix(
        n_samples,
        in.Get(),
        inR.Get(),
        out.Get(),
        outR.Get());

    this->current_plugin_sample += n_samples;

    UpdateOutputControls(n_samples);
}

void ToobLooperEngine::Mix(
    uint32_t n_samples,
    const float *__restrict src,
    const float *__restrict srcR,
    float *__restrict dst,
    float *__restrict dstR)
{
    for (uint32_t i = 0; i < n_samples; ++i)
    {
        dst[i] = src[i];
        dstR[i] = srcR[i];
    }

    for (size_t i = 0; i < loops.size(); ++i)
    {
        loops[i].process(this, src, srcR, dst, dstR, n_samples);
    }

    float lvl = getOutputLevel();
    for (size_t i = 0; i < n_samples; ++i)
    {
        dst[i] *= lvl;
        dstR[i] *= lvl;
    }
}

void ToobLooperFour::Deactivate()
{
    this->activated = false;

    for (size_t i = 0; i < N_LOOPS; ++i)
    {
        loops[i].Reset();
    }
    QuitCommand cmd;
    this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);

    while (true)
    {
        fgHandleMessages();
        if (this->finished)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    this->backgroundThread->join();
    this->backgroundThread.reset();

    super::Deactivate();
}

void ToobLooperEngine::fgHandleMessages()
{
    size_t size = this->fromBackgroundQueue.peekSize();
    if (size == 0)
    {
        return;
    }
    char buffer[2048];
    if (size > sizeof(buffer))
    {
        fgError("Foreground buffer overflow");
        return;
    }
    size_t packetSize = fromBackgroundQueue.read_packet(sizeof(buffer), buffer);
    if (packetSize != 0)
    {
        BufferCommand *cmd = (BufferCommand *)buffer;
        switch (cmd->command)
        {
        case MessageType::BackgroundError:
        {
            BackgroundErrorCommmand *errorCmd = (BackgroundErrorCommmand *)cmd;
            fgError(errorCmd->message);
            break;
        }
        case MessageType::Finished:
        {
            this->finished = true;
            break;
        }
        default:
            fgError("Unknown background message.");
        }
    }
}

void ToobLooperOne::fgError(const char *message)
{
    LogError("%s", message);
}

void ToobLooperFour::fgError(const char *message)
{
    LogError("%s", message);
}

ToobLooperFour::~ToobLooperFour()
{

}

// static size_t CalculateLoopLength(double sampleRate,size_t length, size_t master_loop_length)
// {
//     if (master_loop_length == 0)
//     {
//         return length;
//     }
//     // round up to nearest multiple of master_loop_length (but with 1 second of grace)
//     return ((length + master_loop_length-(size_t)sampleRate) / master_loop_length) * master_loop_length;
// }

size_t ToobLooperEngine::Loop::CalculateCueSamples(size_t masterLoopOffset)
{
    if (masterLoopOffset == 0 || this->master_loop_length == 0)
        return 0;
    return (this->master_loop_length - masterLoopOffset);
}

void ToobLooperEngine::Loop::CancelCue()
{
    if (state == LoopState::CueRecording
        || state == LoopState::TriggerRecording
    )
    {
        state = LoopState::Idle;
        cue_samples = 0;
    }
    if (state == LoopState::CueOverdub)
    {
        state = LoopState::Playing;
    }
}

void ToobLooperEngine::Loop::Init(ToobLooperEngine *plugin)
{
    this->declickSamples = (size_t)(plugin->sampleRate * 0.001); // 1ms of samples.
    this->pre_trigger_samples = (size_t)(plugin->sampleRate * TRIGGER_LEAD_TIME);
    this->pre_trigger_blend_samples = (size_t)(plugin->sampleRate * TRIGGER_FADE_IN_TIME);
    this->bufferSize = plugin->bufferPool->GetBufferSize();
    this->plugin = plugin;
    this->recordLevel.SetSampleRate(plugin->sampleRate);
    this->playbackLevel.SetSampleRate(plugin->sampleRate);
    this->recordLevel.To(0.0f, 0.0);
    this->playbackLevel.To(0.0f, 0.0);
    playError.Init(plugin);
    recordError.Init(plugin);
}

void ToobLooperEngine::Loop::Record(ToobLooperEngine *plugin, size_t loopOffset)
{
    if (state == LoopState::Stopping)
    {
        return;
    }
    if (state == LoopState::CueRecording
        || state == LoopState::TriggerRecording)
    {
        Reset();
        return;
    }
    if (state == LoopState::Playing)
    {
        state = LoopState::Overdubbing;
        this->recordLevel.To(1.0f, TRANSITION_TIME_SEC);

        return;
    }

    if (state == LoopState::Silent)
    {
        this->playbackLevel.To(1.0f, TRANSITION_TIME_SEC);
        this->recordLevel.To(1.0f, TRANSITION_TIME_SEC);
        state = LoopState::Overdubbing;
        return;
    }
    if (state == LoopState::Overdubbing)
    {
        state = LoopState::Playing;
        this->recordLevel.To(0.0f, TRANSITION_TIME_SEC);
        return;
    }
    if (state == LoopState::Recording)
    {
        if (isMasterLoop)
        {
            if (this->length == 0)
            {
                this->length = play_cursor;
            }
            plugin->SetMasterLoopLength(this->length);
            fadeHead();
            recordLevel.To(1.0, 0.0);
            playbackLevel.To(1.0, 0.0);
            this->state = LoopState::Overdubbing;
        }
        else
        {
            this->length = this->master_loop_length; // yyy: Think about this.
            fadeHead();
            recordLevel.To(1.0, 0.0);
            playbackLevel.To(1.0, 0.0);
            this->state = LoopState::Overdubbing;
        }
        if (play_cursor >= length)
        {
            play_cursor = 0;
        }
        return;
    }
    if (state == LoopState::Idle)
    {
        if (master_loop_length == 0)
        { // first loop?
            if (!isMasterLoop)
            {
                recordError.SetError();
                return;
            }
            if (plugin->getTriggerRecord()) {
                state = LoopState::TriggerRecording;
                if (!isMasterLoop) 
                {
                    this->play_cursor = plugin->loops[0].play_cursor;
                }
                return;
            }
            else if (plugin->getEnableRecordCountin())
            {
                plugin->time_zero = plugin->current_plugin_sample;
                plugin->has_time_zero = true;
                recordLevel.To(1.0, 0.0);
                playbackLevel.To(0.0, 0.0);

                state = LoopState::CueRecording;
                play_cursor = loopOffset;
                cue_samples = plugin->GetSamplesPerQuarterNote() * plugin->GetCountInQuarterNotes();
                cue_start = plugin->current_plugin_sample;
            }
            else
            {
                recordLevel.To(1.0, 0.0);
                playbackLevel.To(0.0, 0.0);

                state = LoopState::Recording;
                if (!plugin->has_time_zero)
                {
                    plugin->time_zero = plugin->current_plugin_sample;
                    plugin->has_time_zero = true;
                }
                play_cursor = 0;
            }
            if (plugin->IsFixedLengthLoop())
            {
                this->length = 60.0f * plugin->sampleRate / plugin->getTempo() * quarterNotesPerBar(plugin->getTimesig()) * plugin->getNumberOfBars();
                plugin->SetMasterLoopLength(this->length);
            }
            return;
        }
        else
        {
            if (plugin->getTriggerRecord())
            {
                state = LoopState::TriggerRecording;
            }
            else if (plugin->getRecordSyncOption())
            {
                recordLevel.To(0.0, 0.0);
                playbackLevel.To(0.0, 0.0);
                state = LoopState::CueRecording;

                play_cursor = loopOffset;
                if (play_cursor >= master_loop_length)
                {
                    play_cursor -= master_loop_length;
                    ;
                }
                cue_samples = master_loop_length - play_cursor;
                this->cue_start = plugin->time_zero;
            }
            else
            {
                recordLevel.To(1.0, TRANSITION_TIME_SEC);
                playbackLevel.To(1.0, TRANSITION_TIME_SEC);

                this->state = LoopState::Overdubbing;
                this->play_cursor = loopOffset;
                if (play_cursor == length)
                {
                    play_cursor = 0;
                }
            }
            this->length = master_loop_length;
            return;
        }
        return;
    }
}

void ToobLooperEngine::Loop::Play(ToobLooperEngine *plugin, size_t loopOffset)
{
    if (state == LoopState::Stopping)
    {
        return;
    }
    if (state == LoopState::CueOverdub)
    {
        state = LoopState::Playing;
        cue_samples = 0;
        return;
    }
    CancelCue();
    if (state == LoopState::Recording)
    {
        Record(plugin, loopOffset); // handling is the same.
        if (this->state == LoopState::Overdubbing)
        {
            this->state = LoopState::Playing; // except we end up in play state.
            fadeTail();                       // and we need to de-click the end of the loop.
            this->recordLevel.To(0.0f, TRANSITION_TIME_SEC);
        }
        return;
    }
    if (length == 0)
    {
        playError.SetError();
        state = LoopState::Idle;
        return;
    }
    if (this->state == LoopState::Overdubbing)
    {
        this->state = LoopState::Playing;
        this->recordLevel.To(0.0f, TRANSITION_TIME_SEC);
        return;
    }
    if (state == LoopState::Silent)
    {
        this->state = LoopState::Playing;
        this->playbackLevel.To(1.0f, TRANSITION_TIME_SEC);
        return;
    }
    if (state == LoopState::Playing)
    {
        this->state = LoopState::Silent;
        this->playbackLevel.To(0.0f, TRANSITION_TIME_SEC);
        return;
    }
}

void ToobLooperEngine::Loop::StopInner()
{
    // If we're playing back, fade before resetting.
    if (state == LoopState::Playing || state == LoopState::Silent || state == LoopState::Overdubbing)
    {
        recordLevel.To(0.0f, TRANSITION_TIME_SEC);
        playbackLevel.To(0.0f, TRANSITION_TIME_SEC);
        state = LoopState::Stopping;
    }
    else
    {
        // otherwise reset immediately.
        Reset();
        state = LoopState::Idle;
    }
}

void ToobLooperEngine::Loop::Stop(ToobLooperEngine *plugin, size_t loopOffset)
{
    if (state == LoopState::Stopping)
    {
        return;
    }
    if (this->isMasterLoop)
    {
        // stop ALL the loops.
        for (auto &loop : plugin->loops)
        {
            loop.StopInner();
        }
        plugin->SetMasterLoopLength(0);
        plugin->has_time_zero = false;
    }
    else
    {
        StopInner();
    }
}

void ToobLooperEngine::Loop::Reset()
{
    for (size_t i = 0; i < buffers.size(); ++i)
    {
        if (buffers[i] != nullptr)
        {
            FreeBufferCommand cmd(buffers[i]);
            buffers[i] = nullptr;
            plugin->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
        }
    }
    buffers.clear();
    recordLevel.To(0, 0);
    playbackLevel.To(0, 0);
    play_cursor = 0;
    length = 0;

    state = LoopState::Idle;
    if (isMasterLoop)
    {
        plugin->SetMasterLoopLength(0);
        plugin->has_time_zero = false;
    }
}

void ToobLooperEngine::SetMasterLoopLength(size_t size)
{
    loops[0].master_loop_length = size;
    for (size_t i = 1; i < loops.size(); ++i)
    {
        loops[i].master_loop_length = size;
    }
}

bool ToobLooperFour::GetRecordToOverdubOption()
{
    return this->loop_end_option.GetValue() == 2;
}
bool ToobLooperOne::GetRecordToOverdubOption()
{
    return this->loop_end_option.GetValue() == 2;
}

void ToobLooperEngine::Loop::process(
    ToobLooperEngine *plugin,
    const float *__restrict inL, const float *__restrict inR, float *__restrict outL, float *__restrict outR, size_t n_samples)
{
    size_t index = 0;
    while (index < n_samples)
    {
        switch (state)
        {
        case LoopState::Idle:
            index = n_samples;
            break;
        case LoopState::Recording:
        {
            while (index < n_samples)
            {
                float &vLeft = atL(play_cursor);
                float &vRight = atR(play_cursor);
                float left = inL[index];
                float right = inR[index];
                vLeft = left;
                vRight = right;
                ++play_cursor;
                index++;

                if (play_cursor == length)
                {
                    if (isMasterLoop)
                    {
                        if (plugin->IsFixedLengthLoop())
                        {
                            if (plugin->GetRecordToOverdubOption())
                            {
                                Record(plugin, this->play_cursor);
                            }
                            else
                            {
                                Play(plugin, this->play_cursor);
                            }
                            plugin->OnLoopEnd(*this);
                            break;
                        }
                        else
                        {
                            // keep going!
                        }
                    }
                    else
                    {
                        if (plugin->GetRecordToOverdubOption())
                        {
                            Record(plugin, this->play_cursor);
                        }
                        else
                        {
                            Play(plugin, this->play_cursor);
                        }
                        plugin->OnLoopEnd(*this);
                        break;
                    }
                }
            }
        }
        break;
        case LoopState::Playing:
        case LoopState::Silent:
        case LoopState::Overdubbing:
        case LoopState::Stopping:
        {
            while (index < n_samples)
            {
                float recordLevel = this->recordLevel.Tick();
                float playLevel = this->playbackLevel.Tick();
                float &vLeft = atL(play_cursor);
                float &vRight = atR(play_cursor);
                outL[index] += playLevel * vLeft;
                outR[index] += playLevel * vRight;
                float left = inL[index];
                float right = inL[index];
                vLeft += recordLevel * left;
                vRight += recordLevel * right;

                play_cursor++;
                index++;
                if (play_cursor >= this->length)
                {
                    play_cursor = 0;
                }
            }
            if (state == LoopState::Stopping && playbackLevel.IsComplete())
            {
                Reset(); // fade out complete. Reset the loop and free sample buffers.
                if (isMasterLoop)
                {
                    plugin->SetMasterLoopLength(0);
                    plugin->has_time_zero = false;
                }
            }
            break;
        }
        case LoopState::TriggerRecording:
            {
                if (plugin->inputTrigger.Triggered())
                {
                    size_t triggerPos = plugin->inputTrigger.TriggerFrame();
                    if (index < triggerPos) {
                        index = triggerPos;

                    } 
                    this->state = LoopState::Recording;
                    recordLevel.To(1.0f, 0.0f);
                    if (isMasterLoop)
                    {
                        state = LoopState::Recording;
                        playbackLevel.To(0.0f, 0.0f);
                        CopyInPreTriggerSamples(0,(int64_t)index-(int64_t)n_samples);
                        play_cursor = pre_trigger_samples;

                        if (plugin->IsFixedLengthLoop())
                        {
                            this->length = 60.0f * plugin->sampleRate / plugin->getTempo() * quarterNotesPerBar(plugin->getTimesig()) * plugin->getNumberOfBars();
                            plugin->SetMasterLoopLength(this->length);
                        }
                        if (!plugin->has_time_zero)
                        {
                            plugin->time_zero = plugin->current_plugin_sample + index- pre_trigger_samples;
                            plugin->has_time_zero = true;
                        }
                    } else {
                        play_cursor = plugin->loops[0].play_cursor - n_samples + index;
                        state = LoopState::Overdubbing;
                        playbackLevel.To(1.0f, 0.0f);
                        this->length = plugin->loops[0].length;
                        BlendInPreTriggerSamples(play_cursor,this->length,(int64_t)index-(int64_t)n_samples);
                    }
                    plugin->OnLoopEnd(*this);
                }
                else
                {
                    index = n_samples;
                }
            }
            break;
        case LoopState::CueRecording:
        {
            size_t cueSamples = cue_samples;
            if (cueSamples <= n_samples)
            {
                index += cueSamples;
                cueSamples = 0;
                recordLevel.To(1.0f, 0.0f);
                playbackLevel.To(0.0f, 0.0f);
                state = LoopState::Recording;
                if (isMasterLoop)
                {
                    if (plugin->IsFixedLengthLoop())
                    {
                        this->length = 60.0f * plugin->sampleRate / plugin->getTempo() * quarterNotesPerBar(plugin->getTimesig()) * plugin->getNumberOfBars();
                        plugin->SetMasterLoopLength(this->length);
                    }
                    if (!plugin->has_time_zero)
                    {
                        plugin->time_zero = plugin->current_plugin_sample + index;
                        plugin->has_time_zero = true;
                    }
                }
                play_cursor = 0;
                plugin->OnLoopEnd(*this);
            }
            else
            {
                this->cue_samples -= n_samples - index;
                index = n_samples;
            }
        }
        break;

        default:
            throw std::runtime_error("Unknown loop state.");
        }
    }
}

void ToobLooperEngine::Loop::fadeHead()
{
    size_t nSamples = std::min(this->declickSamples, this->length);
    if (nSamples == 0)
        return;
    for (size_t i = 0; i < nSamples; ++i)
    {
        float fade = (float)i / (float)nSamples;
        atL(i) *= fade;
        atR(i) *= fade;
    }
}
void ToobLooperEngine::Loop::fadeTail()
{
    size_t nSamples = std::min(this->declickSamples, this->length);
    if (nSamples == 0)
        return;
    for (size_t i = 0; i < nSamples; ++i)
    {
        float fadeOut = 1.0f - (float)i / (float)nSamples;
        size_t ix = length - nSamples + i;

        atL(ix) *= fadeOut;
        atR(ix) *= fadeOut;
    }
}

void ToobLooperFour::ErrorBlinker::SetError()
{
    this->hasError = true;
    this->errorTime = plugin->current_plugin_sample;
}

bool ToobLooperFour::ErrorBlinker::ErrorBlinkState()
{
    if (!hasError)
        return false;

    size_t tSample = plugin->current_plugin_sample - errorTime;

    size_t blinkRate = plugin->sampleRate / 4;

    size_t nBlink = tSample / blinkRate;

    bool result = tSample % blinkRate < (blinkRate / 2);
    if (nBlink >= 3 && !result)
    {
        hasError = false;
    }
    return result;
}

bool ToobLooperEngine::IsFixedLengthLoop()
{
    return this->getNumberOfBars() != 0;
}


void ToobLooperOne::HandleTriggers()
{
    bool controlOn = control.GetValue() != 0;
    if (controlOn != lastControlValue)
    {
        lastControlValue = controlOn;

        auto now = clock_t::now();
        this->controlDown = controlOn;
        if (controlOn)
        {
            OnSingleTap();
            lastClickTime = now;
        }
        else
        {
            std::chrono::milliseconds longPressMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime);
            if (longPressMs.count() > 2000)
            {
                OnLongLongPress();
            } else if (longPressMs.count() > 500)
            {
                 OnLongPress();
            }
        }
    }
}

void ToobLooperOne::OnSingleTap()
{
    this->activeLoopsAtTap = this->activeLoops;
    auto &lastLoop = loops[activeLoops - 1];
    switch (pluginState)
    {
    case PluginState::Empty:
        loops[0].Record(this, 0);
        // avoid short first blink due to long-tap update supression.
        record_led.SetValue(true);
        if (loops[0].state == LoopState::Recording)
        {
            pluginState = PluginState::Recording;
        }
        else
        {
            // yyy: What if we're triggering?
            pluginState = PluginState::CueRecording;
        }
        break;
    case PluginState::CueRecording:
        loops[0].CancelCue();
        loops[0].Reset();
        pluginState = PluginState::Empty;
        break;
    case PluginState::Recording:
        lastLoop.Play(this, loops[0].play_cursor);
        pluginState = PluginState::Playing;
        break;
    case PluginState::Playing:
    {
        PushLoop();
        auto &newLoop = loops[activeLoops - 1];
        newLoop.Record(this, loops[0].play_cursor);

        switch (newLoop.state)
        {
        case LoopState::Recording:
            pluginState = activeLoops == 1 ? PluginState::Recording : PluginState::Overdubbing;
            break;
        case LoopState::TriggerRecording:
        case LoopState::CueRecording:
            pluginState = activeLoops == 1 ? PluginState::CueRecording : PluginState::CueOverdubbing;
            break;
        case LoopState::Overdubbing:
            pluginState = PluginState::Overdubbing;
            break;
        case LoopState::CueOverdub:
            pluginState = PluginState::CueOverdubbing;
            break;
        default:
            throw std::runtime_error("Unexpected state.");
        }
    }
    break;
    case PluginState::Overdubbing:
        lastLoop.Play(this, loops[0].play_cursor);
        this->pluginState = PluginState::Playing;
        break;
    case PluginState::CueOverdubbing:
        PopLoop();
        this->pluginState = PluginState::Playing;
        break;
    }
}
void ToobLooperOne::OnLongPress()
{
    lastClickTime = clock_t::now() - std::chrono::milliseconds(10000);

    UndoLoop();
}


void ToobLooperOne::OnLongLongPress()
{
    ResetAll();
}

void ToobLooperOne::UndoLoop()
{
    while (activeLoops >= activeLoopsAtTap && activeLoops > 1)
    {
        PopLoop();
    }
    if (activeLoopsAtTap == 1)
    {
        PopLoop();
    }
}

void ToobLooperOne::OnLoopEnd(Loop &loop)
{
    if (loop.isMasterLoop)
    {
        switch (loop.state)
        {
        case LoopState::Recording:
            pluginState = PluginState::Recording;
            break;
        case LoopState::Overdubbing:
            pluginState = PluginState::Overdubbing;
            break;
        case LoopState::Playing:
            pluginState = PluginState::Playing;
            break;
        default:
            throw std::runtime_error("Unexpected state.");
        }
    }
    else
    {
        switch (loop.state)
        {
        case LoopState::Recording:
            pluginState = PluginState::Overdubbing;
            break;
        case LoopState::Overdubbing:
            pluginState = PluginState::Overdubbing;
            break;
        case LoopState::Playing:
            pluginState = PluginState::Playing;
            break;
        default:
            throw std::runtime_error("Unexpected state.");
        }
    }
}

ToobLooperOne::~ToobLooperOne()
{
}
void ToobLooperOne::ResetAll()
{
    while (activeLoops != 1)
    {
        PopLoop();
    }
    loops[0].Stop(this, loops[0].play_cursor);

    pluginState = PluginState::Empty;
    activeLoops = 1;
}

void ToobLooperEngine::Loop::ControlTap() 
{
    size_t play_cursor = plugin->loops[0].play_cursor;

    switch (state) {
    case LoopState::Idle:
        Record(plugin, play_cursor);
        break;
    case LoopState::CueRecording:
    case LoopState::TriggerRecording:
        CancelCue();
        break;
    case LoopState::Recording:
        Play(plugin, play_cursor);
        break;
    case LoopState::Playing:
        Record(plugin, play_cursor);
        break;
    case LoopState::Overdubbing:
        Play(plugin, play_cursor);
        break;  
    case LoopState::CueOverdub:
        Play(plugin, play_cursor);
        break;
    case LoopState::Stopping:
        Record(plugin,play_cursor);
        break;
    case LoopState::Silent:
        Play(plugin,play_cursor);
        break;
    }
}

void ToobLooperEngine::Loop::ControlLongPress() 
{
    Stop(plugin, plugin->loops[0].play_cursor);
}
void ToobLooperEngine::Loop::ControlDown()
{
    ControlTap();
    lastControlTime = clock_t::now();
}
void ToobLooperEngine::Loop::ControlUp() {
    auto now = clock_t::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now-lastControlTime);
    if (ms.count() > 500) {
        ControlLongPress();
    }   
}

void ToobLooperEngine::Loop::ControlValue(bool value) {
    if (value != lastControlValue) 
    {
        lastControlValue = value;
        if (value) 
        {
            ControlDown();
        } else {
            ControlUp();
        }   
    }
}

void ToobLooperEngine::ProcessInputTrigger(const float*in, const float*inR, size_t n_samples)
{
    size_t maxDelay = trigger_lead_samples + n_samples;
    if (maxDelay > leftInputDelay.GetMaxDelay())
    {
        leftInputDelay.SetMaxDelay(maxDelay);
        rightInputDelay.SetMaxDelay(maxDelay);
    }
    if (inR != nullptr) {
        for (size_t i = 0; i < n_samples; ++i)
        {
            leftInputDelay.Tick(in[i]);
            rightInputDelay.Tick(inR[i]);

        }
    } else {
        for (size_t i = 0; i < n_samples; ++i)
        {
            leftInputDelay.Tick(in[i]);
        }
    }
    inputTrigger.Run(in, inR, n_samples);
}

void ToobLooperEngine::Loop::CopyInPreTriggerSamples(
    size_t play_cursor,int64_t inputDelayOffset
) {

    int32_t inputDelay = (int32_t)this->pre_trigger_samples - (int32_t)inputDelayOffset-1;
    for (size_t i = 0; i < pre_trigger_samples; ++i)
    {
        this->atL(play_cursor+i) = plugin->leftInputDelay.Tap(inputDelay-i);
        this->atR(play_cursor+i) = plugin->rightInputDelay.Tap(inputDelay-i);
    }
}

void ToobLooperEngine::Loop::BlendInPreTriggerSamples(
    size_t play_cursor,size_t length, int64_t inputDelayOffset
) {

    int32_t inputDelay = (int32_t)this->pre_trigger_samples + (int32_t)pre_trigger_blend_samples - (int32_t)inputDelayOffset - 1;
    float blend = 0;
    float dBlend = 1.0f/pre_trigger_blend_samples;
    int64_t outX = (int64_t)play_cursor-this->pre_trigger_blend_samples - this->pre_trigger_samples;
    while (outX < 0) {
        outX += length;
    }
    for (size_t i = 0; i < pre_trigger_blend_samples; ++i) 
    {
        this->atL(outX) += blend*plugin->leftInputDelay.Tap(inputDelay);
        this->atR(outX) += blend*plugin->rightInputDelay.Tap(inputDelay);
        ++outX;
        if (outX > (int64_t)length) outX -= length;
        --inputDelay;
        blend += dBlend;
    }
    for (size_t i = 0; i < pre_trigger_samples; ++i)
    {
        this->atL(outX) += plugin->leftInputDelay.Tap(inputDelay);
        this->atR(outX) += plugin->rightInputDelay.Tap(inputDelay);
        ++outX;
        if (outX > (int64_t)length) outX -= length;
        --inputDelay;
    }
}


REGISTRATION_DECLARATION PluginRegistration<ToobLooperFour> toobLooperFourRegistration(ToobLooperFour::URI);
REGISTRATION_DECLARATION PluginRegistration<ToobLooperOne> toobLooperOneRegistration(ToobLooperOne::URI);
