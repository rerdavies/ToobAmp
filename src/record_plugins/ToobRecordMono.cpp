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

#include "ToobRecordMono.hpp"
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

static REGISTRATION_DECLARATION PluginRegistration<ToobRecordMono> registration(ToobRecordMono::URI);

constexpr char PREFERRED_PATH_SEPARATOR = std::filesystem::path::preferred_separator;

namespace
{
    enum class MessageType
    {
        StartRecording,
        RecordBuffer,
        Stoprecording,

        CuePlayback,
        CuePlaybackResponse,
        RequestNextPlayBuffer,
        NextPlayBufferResponse,
        StartPlayback,
        StopPlayback,

        RecordingStopped,
        BackgroundError,
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

    struct StopPlaybackCommand : public BufferCommand
    {
        StopPlaybackCommand() : BufferCommand(MessageType::StopPlayback, sizeof(StopPlaybackCommand))
        {
        }
    };
    struct StopRecordingCommand : public BufferCommand
    {
        StopRecordingCommand() : BufferCommand(MessageType::Stoprecording, sizeof(StopRecordingCommand))
        {
        }
    };
    struct RecordingStoppedCommand : public BufferCommand
    {
        RecordingStoppedCommand(const char *filename) : BufferCommand(MessageType::RecordingStopped, sizeof(StopRecordingCommand))
        {
            strncpy(this->filename, filename, sizeof(this->filename));
            this->size = sizeof(RecordingStoppedCommand) + strlen(filename) - sizeof(filename) + 1;
            this->size = (this->size + 3) & (~3);
        }

        char filename[1024];
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

    struct ToobStartRecordingCommand : public BufferCommand
    {
        ToobStartRecordingCommand(const std::string &fileName, OutputFormat outputFormat)
            : BufferCommand(MessageType::StartRecording,
                            sizeof(ToobStartRecordingCommand)),
              outputFormat(outputFormat)
        {
            if (fileName.length() > 1023)
            {
                throw std::runtime_error("Filename too long.");
            }
            std::strncpy(this->filename, fileName.c_str(), sizeof(filename));
            this->size = sizeof(ToobStartRecordingCommand) + fileName.length() - sizeof(filename) + 1;
            this->size = (size + 3) & (~3);
        }

        OutputFormat outputFormat;
        char filename[1024];
    };

    constexpr double PREROLL_TIME_SECONDS = 0.5;
    constexpr size_t PREROLL_BUFFERS = (size_t)(PREROLL_TIME_SECONDS / 0.1);

    struct ToobCuePlaybackCommand : public BufferCommand
    {
        ToobCuePlaybackCommand(const std::string &fileName)
            : BufferCommand(MessageType::CuePlayback,
                            sizeof(ToobCuePlaybackCommand))
        {
            if (fileName.length() > 1023)
            {
                throw std::runtime_error("Filename too long.");
            }
            std::strncpy(this->filename, fileName.c_str(), sizeof(filename));
            this->size = sizeof(ToobCuePlaybackCommand) + fileName.length() - sizeof(filename) + 1;
            this->size = (size + 3) & (~3);
        }

        char filename[1024];
    };

    struct ToobNextPlayBufferCommand : public BufferCommand
    {
        ToobNextPlayBufferCommand() : BufferCommand(MessageType::RequestNextPlayBuffer, sizeof(ToobNextPlayBufferCommand))
        {
        }
    };

    struct ToobNextPlayBufferResponseCommand : public BufferCommand
    {
        ToobNextPlayBufferResponseCommand(AudioFileBuffer *buffer)
            : BufferCommand(MessageType::NextPlayBufferResponse, sizeof(ToobNextPlayBufferCommand)), buffer(buffer)
        {
        }
        AudioFileBuffer *buffer = nullptr;
    };

    struct ToobCuePlaybackResponseCommand : public BufferCommand
    {
        ToobCuePlaybackResponseCommand()
            : BufferCommand(MessageType::CuePlaybackResponse,
                            sizeof(ToobCuePlaybackResponseCommand))
        {
            for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
            {
                buffers[i] = nullptr;
            }
        }
        AudioFileBuffer *buffers[PREROLL_BUFFERS];
    };

    struct ToobRecordBufferCommand : BufferCommand
    {
        ToobRecordBufferCommand(AudioFileBuffer *buffer, size_t bufferSize)
            : BufferCommand(MessageType::RecordBuffer, sizeof(ToobRecordBufferCommand)),
              buffer(buffer), bufferSize(bufferSize)
        {
        }

        AudioFileBuffer *buffer;
        size_t bufferSize;
    };
}

ToobRecordMono::ToobRecordMono(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features,
    int channels)
    : super(rate, bundle_path, features)
{


    urids.atom__Path = MapURI(LV2_ATOM__Path);
    urids.atom__String = MapURI(LV2_ATOM__String);

    // reserve space so that we can use the strings without allocating on the realtime thread (unless filenames are unreasonably long).
    filePath.reserve(1024);
    recordingFilePath.reserve(1024);
    recordingDirectory.reserve(1024);

    this->isStereo = channels > 1;

    this->recordingDirectory = "/tmp";
    this->bufferPool = std::make_unique<toob::AudioFileBufferPool>(channels, (size_t)rate / 10);

    this->fileBrowserFilesFeature = this->GetFeature<LV2_FileBrowser_Files>(features, LV2_FILEBROWSER__files);

    if (this->fileBrowserFilesFeature)
    {
        // use the well-known directory for audio file recordings in PiPedal.
        char *cRecordingDirectory = fileBrowserFilesFeature->get_upload_path(
            fileBrowserFilesFeature->handle,
            "audiorecording");
        if (cRecordingDirectory)
        {
            this->recordingDirectory = cRecordingDirectory;

            fileBrowserFilesFeature->free_path(
                fileBrowserFilesFeature->handle,
                cRecordingDirectory);
        }
    }
    if (recordingDirectory.empty())
    {   std::filesystem::path path = getenv("HOME");
        if (!path.empty())
        {
            this->recordingDirectory = "/tmp";
        }

        path = path / "Music/TooB Recordings/";
        this->recordingDirectory = path.string();
    }
    if (!this->recordingDirectory.ends_with(PREFERRED_PATH_SEPARATOR))
    {
        recordingDirectory.append(1, PREFERRED_PATH_SEPARATOR);
    }
    filePath = "";
}

void generate_datetime_filename(char *buffer, size_t buffer_size, const char *extension, const char *prefix = "rec-")
{
    // Get current time
    std::time_t now = std::time(nullptr);

    // Convert to local time
    std::tm *local_time = std::localtime(&now);

    // Ensure we don't overflow the buffer
    if (buffer_size < 32)
    {
        throw std::runtime_error("Buffer size is too small");
    }

    // Format the filename
    int written = std::snprintf(
        buffer,
        buffer_size,
        "%s%04d-%02d-%02d-%02d-%02d-%02d%s",
        prefix,
        local_time->tm_year + 1900, // years since 1900
        local_time->tm_mon + 1,     // months since January (1-12)
        local_time->tm_mday,        // day of the month (1-31)
        local_time->tm_hour,        // hours since midnight (0-23)
        local_time->tm_min,         // minutes after the hour (0-59)
        local_time->tm_sec,         // seconds after the minute (0-60)
        extension);

    // verify if the entire string was written
    if (written < 0 || written >= static_cast<int>(buffer_size))
    {
        throw std::runtime_error("Buffer size is too small");
    }
}

void ToobRecordMono::MakeNewRecordingFilename()
{
    this->recordingFilePath = this->recordingDirectory;

    char cFilename[256];
    generate_datetime_filename(cFilename, sizeof(cFilename), RecordingFileExtension().c_str(), "rec-");

    // GCC: confirmed as zero-allocation by virtue of having reserved memory.
    // You may want to double check this on other compilers. There's always aaloc.
    this->recordingFilePath += cFilename;
}

void ToobRecordMono::Activate()
{
    super::Activate();

    this->activated = true;
    this->finished = false;
    this->state = PluginState::Idle;

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
                case MessageType::StartRecording:
                {
                    ToobStartRecordingCommand* startCmd = (ToobStartRecordingCommand*)cmd;
                    bgStartRecording(startCmd->filename, GetOutputFormat());
                    break;
                }
                case MessageType::RecordBuffer:
                {

                    ToobRecordBufferCommand* recordCmd = (ToobRecordBufferCommand*)cmd;
                    bgWriteBuffer(recordCmd->buffer, recordCmd->bufferSize);


                    bufferPool->PutBuffer(recordCmd->buffer);

                    break;
                }   
                case MessageType::Stoprecording:
                {
                    bgStopRecording();
                    break;
                }  

                case MessageType::CuePlayback:
                {
                    ToobCuePlaybackCommand* cueCmd = (ToobCuePlaybackCommand*)cmd;
                    bgCuePlayback(cueCmd->filename);
                    break;
                }
                case MessageType::RequestNextPlayBuffer:
                {
                    AudioFileBuffer* buffer = bgReadDecoderBuffer();
                    ToobNextPlayBufferResponseCommand responseCommand(buffer);
                    this->fromBackgroundQueue.write_packet(sizeof(responseCommand), (uint8_t*)&responseCommand);
                    break;
                }
                case MessageType::StopPlayback:
                {
                    bgStopPlaying();
                    break;
                }   
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
    bgStopPlaying();
    bgCloseTempFile();

    FinishedCommand finishedCommand;
    this->fromBackgroundQueue.write_packet(sizeof(FinishedCommand), (uint8_t*)&finishedCommand); });
}

void ToobRecordMono::ResetPlayTime()
{
    this->playPosition = 0;
    UpdateOutputControls(0);
}
void ToobRecordMono::UpdateOutputControls(size_t samplesInFrame)
{
    uint64_t time_milliseconds = (uint64_t)(this->playPosition * 1000 / this->getRate());
    this->record_time.SetValue(time_milliseconds * 0.001f, samplesInFrame); // throttled.
    if (state == PluginState::Recording)
    {

        bool ledBlinkStatus = ((time_milliseconds / 300) & 1) == 0;
        this->record_led.SetValue(ledBlinkStatus ? 1.0f : 0.0f);
        this->play_led.SetValue(0);
    }
    else if (state == PluginState::Playing)
    {
        bool ledBlinkStatus = ((time_milliseconds / 300) & 1) == 0;
        this->play_led.SetValue(ledBlinkStatus ? 1.0f : 0.0f);
        this->record_led.SetValue(0);
    }
    else
    {
        this->play_led.SetValue(0);
        this->record_led.SetValue(0);
    }
}

void ToobRecordMono::StartRecording()
{

    this->state = PluginState::Recording;
    ResetPlayTime();
    SetFilePath("");

    UpdateOutputControls(0);

    MakeNewRecordingFilename();

    this->realtimeBuffer.Attach(this->bufferPool->TakeBuffer());
    this->realtimeWriteIndex = 0;

    ToobStartRecordingCommand cmd{this->recordingFilePath, GetOutputFormat()};
    this->toBackgroundQueue.write_packet(cmd.size, (uint8_t *)&cmd);
}

void ToobRecordMono::SendBufferToBackground()
{
    if (this->state == PluginState::Recording)
    {
        auto buffer = this->realtimeBuffer.Detach();

        ToobRecordBufferCommand cmd{buffer, this->realtimeWriteIndex};
        this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
    }
}

void ToobRecordMono::StopRecording()
{

    if (this->state == PluginState::Recording)
    {
        SendBufferToBackground();

        StopRecordingCommand stopCmd;
        this->toBackgroundQueue.write_packet(sizeof(stopCmd), (uint8_t *)&stopCmd);

        this->state = PluginState::Idle;
    }
}
void ToobRecordMono::Run(uint32_t n_samples)
{

    if (this->loadRequested)
    {
        this->loadRequested = false;
        if (!this->filePath.empty())
        { 
            CuePlayback();
        } else {
            StopPlaying();
            StopRecording();
        }
    }

    UpdateOutputControls(n_samples);

    fgHandleMessages();

    if (this->stop.IsTriggered())
    {
        if (this->state == PluginState::Recording)
        {
            StopRecording();
        }
        else if (this->state == PluginState::Playing)
        {
            StopPlaying();
            ResetPlayTime();
        }
        UpdateOutputControls(0);
    }

    if (this->record.IsTriggered())
    {

        if (this->state == PluginState::Recording)
        {
            StopRecording();
            this->state = PluginState::Idle;
            UpdateOutputControls(0);
        }
        else
        {
            StartRecording();
        }
    }
    if (this->play.IsTriggered())
    {
        if (this->state == PluginState::Recording)
        {
            StopRecording();
        }
        if (this->state == PluginState::CuePlaying)
        {
            this->state = PluginState::Playing;
            ResetPlayTime();
            UpdateOutputControls(0);
        }
        else if (this->state == PluginState::Playing)
        {
            StopPlaying();
        }
        else if (this->state == PluginState::Idle)
        {
            CuePlayback();
            if (this->state == PluginState::CuePlaying)
            {
                this->state = PluginState::Playing;
                ResetPlayTime();
            }
            UpdateOutputControls(0);
        }
    }
    Mix(n_samples);
}
void ToobRecordMono::Mix(uint32_t n_samples)
{
    auto src = in.Get();
    auto dst = out.Get();

    auto level = this->level.GetAf();

    for (uint32_t i = 0; i < n_samples; ++i)
    {
        auto value = src[i];
        dst[i] = value;
        this->level_vu.AddValue(value * level);
    }

    if (this->state == PluginState::Recording)
    {
        this->playPosition += n_samples;
        float *buffer = this->realtimeBuffer->GetChannel(0);

        for (uint32_t i = 0; i < n_samples; ++i)
        {
            auto value = src[i];
            dst[i] = value;
            value *= level;
            this->level_vu.AddValue(value * level);

            buffer[this->realtimeWriteIndex] = value;
            this->realtimeWriteIndex++;
            if (this->realtimeWriteIndex >= this->realtimeBuffer->GetBufferSize())
            {
                SendBufferToBackground();


                this->realtimeBuffer.Attach(this->bufferPool->TakeBuffer());
                buffer = this->realtimeBuffer->GetChannel(0);
                this->realtimeWriteIndex = 0;
            }
        }
    }
    if (this->state == PluginState::Playing)
    {
        if (!this->fgPlaybackQueue.empty())
        {
            this->playPosition += n_samples;

            auto buffer = this->fgPlaybackQueue.front();
            float *playData = buffer->GetChannel(0);

            for (uint32_t i = 0; i < n_samples; ++i)
            {
                dst[i] = playData[this->fgPlaybackIndex++];
                if (fgPlaybackIndex == buffer->GetBufferSize())
                {
                    fgPlaybackIndex = 0;
                    fgPlaybackQueue.pop_front();
                    bufferPool->PutBuffer(buffer);
                    if (fgPlaybackQueue.empty())
                    {
                        this->state = PluginState::Idle;
                        CuePlayback();
                        break;
                    }
                    buffer = fgPlaybackQueue.front();
                    playData = buffer->GetChannel(0);

                    ToobNextPlayBufferCommand cmd;
                    this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
                }
            }
        }
    }
}

void ToobRecordStereo::Mix(uint32_t n_samples)
{
    auto srcL = in.Get();
    auto srcR = inR.Get();
    auto dstL = out.Get();
    auto dstR = outR.Get();

    auto level = this->level.GetAf();

    for (uint32_t i = 0; i < n_samples; ++i)
    {
        auto valueL = srcL[i];
        auto valueR = srcR[i];
        dstL[i] = valueL;
        dstR[i] = valueR;
        this->level_vu.AddValue(
            std::max(std::abs(valueL), std::abs(valueR)) 
             * level);
    }

    if (this->state == PluginState::Recording)
    {
        this->playPosition += n_samples;
        float *bufferL = this->realtimeBuffer->GetChannel(0);
        float *bufferR = this->realtimeBuffer->GetChannel(1);

        for (uint32_t i = 0; i < n_samples; ++i)
        {
            auto valueL = srcL[i]*level;
            auto valueR = srcR[i]*level;

            bufferL[this->realtimeWriteIndex] = valueL;
            bufferR[this->realtimeWriteIndex] = valueR;
            this->realtimeWriteIndex++;
            if (this->realtimeWriteIndex >= this->realtimeBuffer->GetBufferSize())
            {
                SendBufferToBackground();


                this->realtimeBuffer.Attach(this->bufferPool->TakeBuffer());
                bufferL = this->realtimeBuffer->GetChannel(0);
                bufferR = this->realtimeBuffer->GetChannel(0);
                this->realtimeWriteIndex = 0;
            }
        }
    }
    if (this->state == PluginState::Playing)
    {
        if (!this->fgPlaybackQueue.empty())
        {
            this->playPosition += n_samples;

            auto buffer = this->fgPlaybackQueue.front();
            float *playDataL = buffer->GetChannel(0);
            float *playDataR = buffer->GetChannel(1);

            for (uint32_t i = 0; i < n_samples; ++i)
            {
                dstL[i] = playDataL[this->fgPlaybackIndex];
                dstR[i] = playDataR[this->fgPlaybackIndex];
                this->fgPlaybackIndex++;

                if (fgPlaybackIndex == buffer->GetBufferSize())
                {
                    fgPlaybackIndex = 0;
                    fgPlaybackQueue.pop_front();
                    bufferPool->PutBuffer(buffer);
                    if (fgPlaybackQueue.empty())
                    {
                        this->state = PluginState::Idle;
                        CuePlayback();

                        break;
                    }
                    buffer = fgPlaybackQueue.front();
                    playDataL = buffer->GetChannel(0);
                    playDataR = buffer->GetChannel(1);

                    ToobNextPlayBufferCommand cmd;
                    this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
                }
            }
        }
    }
}

void ToobRecordMono::Deactivate()
{
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

    bgCloseTempFile();
    bgStopPlaying();

    while (!fgPlaybackQueue.empty()) {
        bufferPool->PutBuffer(fgPlaybackQueue.pop_front());
    }
    this->fgPlaybackIndex = 0;

    this->state = PluginState::Idle;

    this->activated = false;
    super::Deactivate();
}

bool ToobRecordMono::OnPatchPathSet(LV2_URID propertyUrid, const char *value)
{
    if (propertyUrid == this->audioFile_urid)
    {
        SetFilePath(value);
        this->CuePlayback(value);
        return true;
    }
    return false;
}
const char *ToobRecordMono::OnGetPatchPropertyValue(LV2_URID propertyUrid)
{
    if (propertyUrid == this->audioFile_urid)
    {
        return this->filePath.c_str();
    }
    return nullptr;
}

void ToobRecordMono::SetFilePath(const char *filename)
{
    if (strcmp(filename, this->filePath.c_str()) == 0)
        return;
    this->filePath = filename;
    if (activated)
    {
        this->PutPatchPropertyPath(0, this->audioFile_urid, filename);
    }
}

void ToobRecordMono::StopPlaying()
{
    if (this->state == PluginState::Playing || this->state == PluginState::CuePlaying)
    {
        this->state = PluginState::Idle;
        StopPlaybackCommand cmd;
        this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
    }
    CuePlayback();
}

void ToobRecordMono::CuePlayback()
{
    if (activated)
    {
        if (this->filePath.empty())
        {
            return;
        }
        CuePlayback(this->filePath.c_str());
    }
}

void ToobRecordMono::CuePlayback(const char *filename)
{
    if (activated)
    {
        if (this->state == PluginState::Recording)
        {
            StopRecording();
        }
        SetFilePath(filename);

        this->state = PluginState::CuePlaying;

        ToobCuePlaybackCommand cmd{filename};
        this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
    }
}

void ToobRecordMono::bgCloseTempFile()
{
    if (bgFile)
    {
        fclose(bgFile);
        bgFile = nullptr;
    }
    if (bgTemporaryFile)
    {
        bgTemporaryFile.reset();
    }
}

void ToobRecordMono::bgStartRecording(const char *filename, OutputFormat outputFormat)
{
    bgStopPlaying();
    bgCloseTempFile();
    
    bufferPool->Reserve(10); // nominally up to ~1 second of buffering (with 0.5s pre-roll)
    this->bgRecordingFilePath = filename;
    this->bgOutputFormat = outputFormat;

    bgTemporaryFile = std::make_unique<pipedal::TemporaryFile>(bgRecordingFilePath.parent_path(), ".$$$");
    FILE *file = fopen(bgTemporaryFile->Path().c_str(), "wb");
    if (!file)
    {
        throw std::runtime_error("Failed to open temporary file for recording.");
    }
    this->bgFile = file;
}
void ToobRecordMono::bgWriteBuffer(toob::AudioFileBuffer *buffer, size_t count)
{

    if (!bgFile)
    {
        return;
    }
    size_t channels = buffer->GetChannelCount();
    if (channels == 1)
    {
        float *data = buffer->GetChannel(0);
        fwrite(data, sizeof(float), count, bgFile);
    }
    else if (channels == 2)
    {
        float rawBuffer[1024];

        size_t offset = 0;

        while (count != 0)
        {
            size_t thisTime = std::min(count, (size_t)(1024 / 2));
            float *data0 = buffer->GetChannel(0) + offset;
            float *data1 = buffer->GetChannel(1) + offset;
            size_t ix = 0;
            for (size_t i = 0; i < thisTime; i++)
            {
                rawBuffer[ix++] = data0[i];
                rawBuffer[ix++] = data1[i];
            }
            size_t written = fwrite(rawBuffer, sizeof(float), thisTime * 2, bgFile);
            if (written != thisTime * 2)
            {
                std::stringstream ss;
                ss << "Failed to write to temporary file. " << strerror(errno);
                bgCloseTempFile();
                throw std::runtime_error(ss.str());
            }
            count -= thisTime;
            offset += thisTime;
        }
    }
}

static std::string execForOutput(const char *cmd)
{
    std::array<char, 128> buffer;
    std::string result;

    FILE *pipe = popen(cmd, "r");

    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result += buffer.data();
    }
    int rc = pclose(pipe);

    bool success = WIFEXITED(rc) && WEXITSTATUS(rc) == EXIT_SUCCESS;

    if (!success)
    {
        throw std::runtime_error("Command to execute ffmpeg conversion. " + std::string(cmd) + " " + result);
    }

    return result;
}

static std::string fileToCmdline(const std::filesystem::path &path)
{
    std::string t = path.string();

    std::stringstream ss;
    ss << '\'';
    for (char c : t)
    {
        if (c == '\'')
        {
            ss << "'\\''";
        }
        else
        {
            ss << c;
        }
    }
    ss << '\'';
    return ss.str();
}

void ToobRecordMono::bgStopRecording()
{
    if (bgFile)
    {
        fclose(bgFile);
        bgFile = nullptr;

        std::stringstream s;
        // ffmpeg -f f32le -ar 48000 -ac 2 -i rawfile.raw -c:a flac -compression_level 12 output.flac

        std::string encodingArgs;
        std::string extension;
    switch (bgOutputFormat)
        {
        case OutputFormat::Wav:
        {
            // encodingArgs = "-acodec pcm_f32le";
            encodingArgs = "-acodec pcm_s16le";
            extension = ".wav";
            break;
        }
        case OutputFormat::WavFloat:
        {
            encodingArgs = "-acodec pcm_f32le";
            extension = ".wav";
            break;
        }
        case OutputFormat::Flac:
        {
            encodingArgs = "-c:a flac  -sample_fmt s32 -compression_level 12";
            extension = ".flac";
            break;
        }
        case OutputFormat::Mp3:
        {
            encodingArgs = "-codec:a libmp3lame -qscale:a 0";
            extension = ".mp3";
            break;
        }
        }

        s << "/usr/bin/ffmpeg -y -f f32le -ar " << ((size_t)getRate()) << " -ac " << (isStereo ? 2 : 1)
          << " -i " << fileToCmdline(bgTemporaryFile->Path())
          << " " << encodingArgs
          << " " << fileToCmdline(bgRecordingFilePath) << " 2>&1";

        execForOutput(s.str().c_str());
    }
    bgCloseTempFile();

    RecordingStoppedCommand cmd(this->bgRecordingFilePath.c_str());
    this->fromBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
}

OutputFormat ToobRecordMono::GetOutputFormat()
{
    return (OutputFormat)fformat.GetValue();
}

std::string ToobRecordMono::RecordingFileExtension()
{
    switch (GetOutputFormat())
    {
    case OutputFormat::Wav:
    case OutputFormat::WavFloat:
        return ".wav";
    case OutputFormat::Flac:
        return ".flac";
    case OutputFormat::Mp3:
        return ".mp3";
    }
    return ".wav";
}

void ToobRecordMono::fgHandleMessages()
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
        case MessageType::RecordingStopped:
        {
            RecordingStoppedCommand *recordingStoppedCommand = (RecordingStoppedCommand *)cmd;
            this->state = PluginState::Idle;
            SetFilePath(recordingStoppedCommand->filename);
            CuePlayback(recordingStoppedCommand->filename);
            break;
        }
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
        case MessageType::CuePlaybackResponse:
        {
            fgResetPlaybackQueue();
            ToobCuePlaybackResponseCommand *responseCommand = (ToobCuePlaybackResponseCommand *)cmd;
            if (this->state == PluginState::CuePlaying || this->state == PluginState::Playing)
            {
                fgPlaybackIndex = 0;

                for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
                {
                    if (responseCommand->buffers[i])
                    {
                        fgPlaybackQueue.push_back(responseCommand->buffers[i]);
                    }
                }
            }
            else
            {
                // return them to the buffer pull.
                fgPlaybackIndex = 0;
                for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
                {
                    if (responseCommand->buffers[i])
                    {
                        bufferPool->PutBuffer(responseCommand->buffers[i]);
                    }
                }
            }
            break;
        }
        case MessageType::NextPlayBufferResponse:
        {
            ToobNextPlayBufferResponseCommand *responseCommand = (ToobNextPlayBufferResponseCommand *)cmd;
            if (responseCommand->buffer)
            {
                if (this->state == PluginState::Playing)
                {
                    fgPlaybackQueue.push_back(responseCommand->buffer);
                }
                else
                {
                    bufferPool->PutBuffer(responseCommand->buffer);
                }
            }
            break;
        }
        default:
            fgError("Unknown background message.");
        }
    }
}

void ToobRecordMono::fgError(const char *message)
{
    if (this->state != PluginState::Error)
    {
        this->state = PluginState::Error;
        LogError("%s", message);
    }
    SetFilePath("");
}

AudioFileBuffer *ToobRecordMono::bgReadDecoderBuffer()
{
    if (!this->decoderStream)
    {
        return nullptr;
    }
    AudioFileBuffer *buffer = bufferPool->TakeBuffer();
    size_t channels = buffer->GetChannelCount();
    size_t count = buffer->GetBufferSize();

    if (channels == 1)
    {
        float *data = buffer->GetChannel(0);
        float *buffers[1] = {data};
        //
        auto nRead = this->decoderStream->read(buffers, count);
        if (nRead != count)
        {
            for (size_t i = nRead; i < count; ++i)
            {
                data[i] = 0;
            }
            decoderStream.reset();
        }
        return buffer;
    }
    else if (channels == 2)
    {
        float *data0 = buffer->GetChannel(0);
        float *data1 = buffer->GetChannel(1);

        float *buffers[2] = {data0, data1};
        //
        auto nRead = this->decoderStream->read(buffers, count);
        if (nRead != count)
        {
            for (size_t i = nRead; i < count; ++i)
            {
                data0[i] = 0;
                data1[i] = 0;
            }
            decoderStream.reset();
        }
        return buffer;
    }
    else
    {
        throw std::runtime_error("Unsupported number of channels.");
    }
}

void ToobRecordMono::bgCuePlayback(const char *filename)
{

    try
    {
        this->decoderStream = std::make_unique<FfmpegDecoderStream>();
        decoderStream->open(filename, isStereo ? 2 : 1, (uint32_t)getRate());
    }
    catch (const std::exception &e)
    {
        this->decoderStream.reset();
        BackgroundErrorCommmand errorCmd(e.what());
        this->fromBackgroundQueue.write_packet(sizeof(errorCmd), (uint8_t *)&errorCmd);
        return;
    }
    ToobCuePlaybackResponseCommand responseCommand;

    // buffers are about 1/10 second of data.
    // Prepare 1/2 second of pre-roll data.
    for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
    {
        responseCommand.buffers[i] = bgReadDecoderBuffer();
    }
    this->fromBackgroundQueue.write_packet(sizeof(responseCommand), (uint8_t *)&responseCommand);
}

void ToobRecordMono::bgStopPlaying()
{
    this->decoderStream.reset();
}

void ToobRecordMono::fgResetPlaybackQueue()
{
    while (!fgPlaybackQueue.empty())
    {
        bufferPool->PutBuffer(fgPlaybackQueue.pop_front());
    }
    fgPlaybackIndex = 0;
}

ToobRecordStereo::ToobRecordStereo(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : super(rate, bundle_path, features,2)
{
    this->isStereo = true;

}

ToobRecordMono::~ToobRecordMono() {

}

LV2_State_Status
ToobRecordMono::OnRestoreLv2State(
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    std::string modelFileName;

    {
        size_t size;
        uint32_t type;
        uint32_t flags;
        const void *data = (*retrieve)(handle, this->audioFile_urid, &size, &type, &flags);
        if (data)
        {
            if (type != this->urids.atom__Path && type != this->urids.atom__String)
            {
                return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
            }
            modelFileName = MapFilename(features, (const char *)data, nullptr);
            RequestLoad(modelFileName.c_str());
        }
    }

    return LV2_State_Status::LV2_STATE_SUCCESS;
}


LV2_State_Status
ToobRecordMono::OnSaveLv2State(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    if (this->filePath.empty()) 
    {
        return LV2_State_Status::LV2_STATE_SUCCESS; // not-set => "". Avoids assuming that hosts can handle a "" path.
    }
    std::string abstractPath = this->UnmapFilename(features, this->filePath.c_str());

    store(handle, audioFile_urid , abstractPath.c_str(), abstractPath.length() + 1, urids.atom__Path, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    return LV2_State_Status::LV2_STATE_SUCCESS;
}


std::string ToobRecordMono::UnmapFilename(const LV2_Feature *const *features, const std::string &fileName)
{
    // const LV2_State_Make_Path *makePath = GetFeature<LV2_State_Make_Path>(features, LV2_STATE__makePath);
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath)
    {
        char *result = mapPath->abstract_path(mapPath->handle, fileName.c_str());
        std::string t = result;
        if (freePath)
        {
            freePath->free_path(freePath->handle, result);
        }
        else
        {
            free(result);
        }
        return t;
    }
    else
    {
        return fileName;
    }
}

std::string ToobRecordMono::MapFilename(
    const LV2_Feature *const *features,
    const std::string &input,
    const char *browserPath)
{
    if (input.starts_with(this->GetBundlePath().c_str()))
    {
        // map bundle files to corresponding files in the browser dialog directories.
        const LV2_FileBrowser_Files *browserFiles = GetFeature<LV2_FileBrowser_Files>(features, LV2_FILEBROWSER__files);
        if (browserFiles != nullptr)
        {
            char *t = nullptr;
            t = browserFiles->map_path(browserFiles->handle, input.c_str(), "impulseFiles/reverb", browserPath);
            std::string result = t;
            browserFiles->free_path(browserFiles->handle, t);
            return result;
        }
        return input;
    }
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath == nullptr)
    {
        return input;
    }
    else
    {
        char *t = mapPath->absolute_path(mapPath->handle, input.c_str());
        std::string result = t;
        if (freePath)
        {
            freePath->free_path(freePath->handle, t);
        }
        else
        {
            free(t);
        }
        return result;
    }
}


void ToobRecordMono::RequestLoad(const char *filename)
{
    this->filePath = filename;
    this->loadRequested = true;
}



REGISTRATION_DECLARATION PluginRegistration<ToobRecordMono> toobMonoregistration(ToobRecordMono::URI);
REGISTRATION_DECLARATION PluginRegistration<ToobRecordStereo> toobStereoRegistration(ToobRecordStereo::URI);
