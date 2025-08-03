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

#include "Lv2AudioFileProcessor.hpp"
#include <cstring>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <memory>

#include "FfmpegDecoderStream.hpp"

#include "../LsNumerics/LsMath.hpp"

using namespace toob;

namespace
{

    

    enum class MessageType
    {
        StartRecording,
        RecordBuffer,
        Stoprecording,

        CuePlayback,
        SetLoopParameters,
        CuePlaybackResponse,
        DeleteLoopBuffer,
        RequestNextPlayBuffer,
        NextPlayBufferResponse,
        StartPlayback,
        StopPlayback,

        UpdateLoopParameters,
        RecordingStopped,
        BackgroundError,
        Quit,
        Finished
    };

    struct BufferMessage
    {
        BufferMessage(MessageType command, size_t size) : size(size), command(command)
        {
        }
        size_t size;
        MessageType command;
    };

    struct StopPlaybackMessage : public BufferMessage
    {
        StopPlaybackMessage() : BufferMessage(MessageType::StopPlayback, sizeof(StopPlaybackMessage))
        {
        }
    };
    struct StopRecordingMessage : public BufferMessage
    {
        StopRecordingMessage() : BufferMessage(MessageType::Stoprecording, sizeof(StopRecordingMessage))
        {
        }
    };
    struct RecordingStoppedMessage : public BufferMessage
    {
        RecordingStoppedMessage(const char *filename) : BufferMessage(MessageType::RecordingStopped, sizeof(StopRecordingMessage))
        {
            strncpy(this->filename, filename, sizeof(this->filename));
            this->size = sizeof(RecordingStoppedMessage) + strlen(filename) - sizeof(filename) + 1;
            this->size = (this->size + 3) & (~3);
        }

        char filename[1024];
    };

    struct QuitMessage : public BufferMessage
    {
        QuitMessage() : BufferMessage(MessageType::Quit, sizeof(QuitMessage))
        {
        }
    };
    struct FinishedMessage : public BufferMessage
    {
        FinishedMessage() : BufferMessage(MessageType::Finished, sizeof(QuitMessage))
        {
        }
    };

    struct BackgroundErrorCommmand : public BufferMessage
    {
        BackgroundErrorCommmand(const std::string &message) : BufferMessage(MessageType::BackgroundError, sizeof(BackgroundErrorCommmand))
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

    struct UpdateLoopParametersCommand : public BufferMessage
    {
        UpdateLoopParametersCommand(
            uint64_t operationId,
            const char*loopJson,
            double seekPosSeconds,
            double duration
        )
            : BufferMessage(MessageType::UpdateLoopParameters, sizeof(UpdateLoopParametersCommand)),
                operationId(operationId),
              seekPosSeconds(seekPosSeconds),
              duration(duration)
        {
            size_t len = strlen(loopJson) + 1;
            this->size = sizeof(UpdateLoopParametersCommand);
            this->size = sizeof(UpdateLoopParametersCommand)-sizeof(this->loopJson) + len;
            if (this->size > sizeof(UpdateLoopParametersCommand))
            {
                throw std::runtime_error("Command size exceeds structure size");
            }
            this->size = (this->size + 3) & (~3);
            memcpy(this->loopJson, loopJson, len);
        }

        uint64_t operationId = (uint64_t)-1;
        double seekPosSeconds = 0.0; 
        double duration = 0.0; 
        char loopJson[1024] = {0};
    };  
    struct ToobStartRecordingMessage : public BufferMessage
    {
        ToobStartRecordingMessage(const std::string &fileName, OutputFormat outputFormat)
            : BufferMessage(MessageType::StartRecording,
                            sizeof(ToobStartRecordingMessage)),
              outputFormat(outputFormat)
        {
            if (fileName.length() > 1023)
            {
                throw std::runtime_error("Filename too long.");
            }
            std::strncpy(this->filename, fileName.c_str(), sizeof(filename));
            this->size = sizeof(ToobStartRecordingMessage) + fileName.length() - sizeof(filename) + 1;
            this->size = (size + 3) & (~3);
        }

        OutputFormat outputFormat;
        char filename[1024];
    };

    struct ToobCuePlaybackMessage : public BufferMessage
    {
        ToobCuePlaybackMessage(uint64_t operationId, const char *fileNameInput, size_t seekPos)
            : BufferMessage(MessageType::CuePlayback,
                            sizeof(ToobCuePlaybackMessage)),
              operationId(operationId),
              seekPos(seekPos)
        {
            size_t fileNameLen = strlen(fileNameInput);
            if (fileNameLen + 1 > sizeof(this->buffer))
            {
                throw std::runtime_error("Filename too long.");
            }
            std::memcpy(this->buffer, fileNameInput, fileNameLen + 1);
            this->size = sizeof(ToobCuePlaybackMessage) - sizeof(this->buffer) + fileNameLen + 1;

            // Add final size check
            if (this->size > sizeof(ToobCuePlaybackMessage))
            {
                throw std::runtime_error("Command size exceeds structure size");
            }

            // Align to 4 bytes
            this->size = (size + 3) & (~3);
        }

        const char *getFileName() const
        {
            return this->buffer;
        }
        uint64_t operationId = (size_t)-1;
        size_t seekPos = 0;

    private:
        char buffer[1024];
    };
struct SetLoopParametersMessage : public BufferMessage
    {
        SetLoopParametersMessage(uint64_t operationId, const char*fileName,const char *loopJson)
            : BufferMessage(MessageType::SetLoopParameters,
                            sizeof(SetLoopParametersMessage)),
              operationId(operationId)
        {
            size_t fileNameLen = strlen(fileName);
            size_t jsonLen  = strlen(loopJson);

            this->size = sizeof(SetLoopParametersMessage) - sizeof(this->buffer) + fileNameLen + 1 + jsonLen + 1;

            if (this->size > sizeof(SetLoopParametersMessage))
            {
                throw std::runtime_error("Command size exceeds structure size");
            }

            std::memcpy(this->buffer, fileName, fileNameLen+1);
            this->loopOffset = fileNameLen + 1;
            std::memcpy(this->buffer+loopOffset, loopJson, jsonLen + 1);

            // Align to 4 bytes
            this->size = (size + 3) & (~3);
        }

        const char *getFilename() const
        {
            return this->buffer;
        }
        const char *getLoopJson() const
        {
            return this->buffer+loopOffset;
        }
        uint64_t operationId = (uint64_t)-1;

    private:
        size_t loopOffset = 0;
        char buffer[2048];
    };
    struct ToobDeleteLoopBufferMessage : public BufferMessage
    {
        ToobDeleteLoopBufferMessage(toob::AudioFileBuffer *buffer)
            : BufferMessage(MessageType::DeleteLoopBuffer, sizeof(ToobDeleteLoopBufferMessage)),
              buffer(buffer)
        {
        }
        toob::AudioFileBuffer *buffer;
    };

    struct ToobNextPlayBufferMessage : public BufferMessage
    {
        ToobNextPlayBufferMessage(uint64_t operationId)
            : BufferMessage(MessageType::RequestNextPlayBuffer, sizeof(ToobNextPlayBufferMessage)),
              operationId(operationId)
        {
        }
        uint64_t operationId = 0; // used to detect cancelled i/o requests.
    };

    struct ToobNextPlayBufferResponseMessage : public BufferMessage
    {
        ToobNextPlayBufferResponseMessage(uint64_t operationId, AudioFileBuffer *buffer)
            : BufferMessage(MessageType::NextPlayBufferResponse,
                            sizeof(ToobNextPlayBufferMessage)),
              operationId(operationId),
              buffer(buffer)
        {
        }
        uint64_t operationId = 0;
        AudioFileBuffer *buffer = nullptr;
    };

    struct ToobCuePlaybackResponseMessage : public BufferMessage
    {
        ToobCuePlaybackResponseMessage(
            uint64_t operationId,
            size_t seekPos,
            const LoopParameters &loopParameters,
            double duration,
            const char* loopParameterJson)
            : BufferMessage(MessageType::CuePlaybackResponse,
                            sizeof(ToobCuePlaybackResponseMessage)),
              operationId(operationId),
              seekPos(seekPos),
              duration(duration),
              loopParameters(loopParameters)
        {
            for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
            {
                buffers[i] = nullptr;
            }
            size = sizeof(ToobCuePlaybackResponseMessage)-sizeof(this->loopParameterJson) + strlen(loopParameterJson) + 1;
            if (size > sizeof(ToobCuePlaybackResponseMessage))
            {
                throw std::runtime_error("Command size exceeds structure size");
            }
            size = (size + 3) & (~3);
            strncpy(this->loopParameterJson, loopParameterJson, sizeof(this->loopParameterJson) - 1);
        }
        uint64_t operationId;
        size_t seekPos;
        double duration;
        LoopParameters loopParameters;
        size_t bufferCount = 0;
        AudioFileBuffer *buffers[PREROLL_BUFFERS];
        AudioFileBuffer *loopBuffer = nullptr;
        char loopParameterJson[1024] = {0};
    };

    struct ToobRecordBufferMessage : BufferMessage
    {
        ToobRecordBufferMessage(AudioFileBuffer *buffer, size_t bufferSize)
            : BufferMessage(MessageType::RecordBuffer, sizeof(ToobRecordBufferMessage)),
              buffer(buffer), bufferSize(bufferSize)
        {
        }

        AudioFileBuffer *buffer;
        size_t bufferSize;
    };

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

LoopType toob::GetLoopType(const LoopParameters &loopParameters, float sampleRate)
{
    if (loopParameters.loopEnable_)
    {
        if (loopParameters.loopStart_ == loopParameters.loopEnd_)
        {
            return LoopType::None;
        }
        double first = std::min(loopParameters.loopStart_, loopParameters.start_);
        double last = loopParameters.loopEnd_;

        if (last - first < 10.0)
        {
            return LoopType::SmallLoop;
        }
        if (loopParameters.loopEnd_ - loopParameters.loopStart_ < 10)

        {
            return LoopType::BigStartSmallLoop;
        }
        return LoopType::BigLoop;
    }
    return LoopType::None;
}

size_t toob::GetLoopBlendLength(double sampleRate)
{
    constexpr double LOOP_BLEND_TIME_SECONDS = 0.025;
    return (size_t)std::ceil(sampleRate * LOOP_BLEND_TIME_SECONDS);
}

void Lv2AudioFileProcessor::Activate()
{
    activated = true;
    this->fgFinished = false;

    SetDbVolume(this->dbVolume, this->pan, true); // apply mix immediatedly.

    this->backgroundThread = std::make_unique<std::jthread>(
        [this]()
        {
            try
            {
                bool quit = false;

                std::vector<uint8_t> buffer(2048);
                BufferMessage *cmd = (BufferMessage *)buffer.data();
                while (!quit)
                {
                    this->toBackgroundQueue.readWait();
                    size_t size = this->toBackgroundQueue.peekSize();
                    if (size == 0)
                        continue;

                    if (size > buffer.size())
                    {
                        buffer.resize(size);
                        cmd = (BufferMessage *)buffer.data();
                    }
                    if (!this->toBackgroundQueue.read_packet(buffer.size(), (uint8_t *)cmd))
                    {
                        break;
                    }

                    try
                    {
                        switch (cmd->command)
                        {
                        case MessageType::StartRecording:
                        {
                            ToobStartRecordingMessage *startCmd = (ToobStartRecordingMessage *)cmd;
                            bgStartRecording(startCmd->filename, startCmd->outputFormat);
                            break;
                        }
                        case MessageType::RecordBuffer:
                        {

                            ToobRecordBufferMessage *recordCmd = (ToobRecordBufferMessage *)cmd;
                            bgWriteBuffer(recordCmd->buffer, recordCmd->bufferSize);

                            bufferPool->PutBuffer(recordCmd->buffer);

                            break;
                        }
                        case MessageType::Stoprecording:
                        {
                            bgStopRecording();
                            break;
                        }
                        case MessageType::SetLoopParameters:
                        {

                            SetLoopParametersMessage *setLoopCmd = (SetLoopParametersMessage *)cmd;
                            bgSetLoopParameters(setLoopCmd->operationId, setLoopCmd->getFilename(),setLoopCmd->getLoopJson());
                            break;
                        }

                        case MessageType::CuePlayback:
                        {
                            ToobCuePlaybackMessage *cueCmd = (ToobCuePlaybackMessage *)cmd;
                            bgCuePlayback(cueCmd->operationId, cueCmd->getFileName(), cueCmd->seekPos);
                            break;
                        }
                        case MessageType::DeleteLoopBuffer:
                        {
                            ToobDeleteLoopBufferMessage *deleteCmd = (ToobDeleteLoopBufferMessage *)cmd;
                            if (deleteCmd->buffer)
                            {
                                deleteCmd->buffer->Release();
                            }
                            break;
                        }
                        case MessageType::RequestNextPlayBuffer:
                        {
                            ToobNextPlayBufferMessage *nextCmd = (ToobNextPlayBufferMessage *)cmd;
                            if (nextCmd->operationId == fgOperationId)
                            {
                                AudioFileBuffer *buffer = bgReadDecoderBuffer();
                                ToobNextPlayBufferResponseMessage responseCommand(nextCmd->operationId, buffer);
                                this->fromBackgroundQueue.write_packet(sizeof(responseCommand), (uint8_t *)&responseCommand);
                            }
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
                    }
                    catch (const std::exception &e)
                    {
                        std::stringstream ss;
                        ss << "Background thread error: " << e.what();
                        BackgroundErrorCommmand errorCmd(ss.str());
                        this->fromBackgroundQueue.write_packet(errorCmd.size, (uint8_t *)&errorCmd);
                    }
                }
            }
            catch (std::exception &e)
            {
                std::stringstream ss;
                ss << "Background thread error: " << e.what();
                BackgroundErrorCommmand errorCmd(ss.str());
                this->fromBackgroundQueue.write_packet(errorCmd.size, (uint8_t *)&errorCmd);
            }
            bgStopPlaying();
            bgCloseTempFile();

            FinishedMessage finishedCommand;
            this->fromBackgroundQueue.write_packet(sizeof(FinishedMessage), (uint8_t *)&finishedCommand);
        });
}
void Lv2AudioFileProcessor::Deactivate()
{
    if (!activated)
    {
        return;
    }

    activated = false;
    QuitMessage cmd;
    this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);

    while (true)
    {
        HandleMessages();
        if (this->fgFinished)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    this->backgroundThread->join();
    this->backgroundThread.reset();

    fgLoopBuffer = nullptr;
    while (!fgPlaybackQueue.empty())
    {
        bufferPool->PutBuffer(fgPlaybackQueue.pop_front());
    }

    bgCloseTempFile();
    bgStopPlaying();
}

void Lv2AudioFileProcessor::fgDeleteLoopBuffer(toob::AudioFileBuffer *buffer)
{
    if (buffer)
    {
        ToobDeleteLoopBufferMessage cmd(buffer);
        this->toBackgroundQueue.write_packet(cmd.size, (uint8_t *)&cmd);
    }
}

void Lv2AudioFileProcessor::fgRequestNextPlayBuffer()
{
    ToobNextPlayBufferMessage cmd(this->fgOperationId);
    this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
}

void Lv2AudioFileProcessor::StopPlayback()
{
    if (this->state == ProcessorState::Playing || this->state == ProcessorState::Paused ||
        this->state == ProcessorState::CuePlayingThenPlay || this->state == ProcessorState::CuePlayingThenPause)
    {
        fgStopPlaying();
        SetState(ProcessorState::Idle);
        ++fgOperationId; // cancel pending requests.
        fgResetPlaybackQueue();
    }
}

void Lv2AudioFileProcessor::Stop()
{
    switch (this->state)
    {
    case ProcessorState::Playing:
    case ProcessorState::Paused:
    case ProcessorState::CuePlayingThenPlay:
    case ProcessorState::CuePlayingThenPause:
        StopPlayback();
        break;
    case ProcessorState::StoppingRecording:
        // Have to wati for the background task to finish.
        return;
    case ProcessorState::Recording:
        StopRecording();
        return; // in StoppingRecording state.
    case ProcessorState::Error:
    case ProcessorState::Idle:
        SetState(ProcessorState::Idle);
        return;
    }
}
void Lv2AudioFileProcessor::StartRecording(const std::string &recordingFilePath, OutputFormat recordFormat)
{

    if (this->state == ProcessorState::Recording)
    {
        return; // already recording.
    }

    Stop();
    if (this->state != ProcessorState::Idle)
    { // Can't start recording because there is a pending i/o operation.
        return;
    }

    playAfterRecording = false;

    this->realtimeRecordBuffer = nullptr;
    this->realtimeRecordBuffer.attach(this->bufferPool->TakeBuffer());
    this->realtimeWriteIndex = 0;
    this->playPosition = 0;

    fgStartRecording(recordingFilePath, recordFormat);
    SetState(ProcessorState::Recording);
}

void Lv2AudioFileProcessor::StopRecording()
{
    if (this->state == ProcessorState::Recording)
    {
        SendBufferToBackground();
        fgStopRecording();
        SetState(ProcessorState::StoppingRecording);
    }
}

void Lv2AudioFileProcessor::SendBufferToBackground()
{
    if (this->realtimeRecordBuffer.Get() != nullptr)
    {
        this->realtimeRecordBuffer->SetBufferSize(this->realtimeWriteIndex);
        toob::AudioFileBuffer *buffer = this->realtimeRecordBuffer.detach();
        buffer->SetBufferSize(this->realtimeWriteIndex);

        fgRecordBuffer(buffer, this->realtimeWriteIndex);

        this->realtimeWriteIndex = 0;
    }
}

void Lv2AudioFileProcessor::fgStartRecording(const std::string &recordingFilePath, OutputFormat recordFormat)
{
    ToobStartRecordingMessage cmd{recordingFilePath, recordFormat};
    this->toBackgroundQueue.write_packet(cmd.size, (uint8_t *)&cmd);
}
void Lv2AudioFileProcessor::fgRecordBuffer(toob::AudioFileBuffer *buffer, size_t count)
{
    ToobRecordBufferMessage cmd{buffer, count};
    this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
}
void Lv2AudioFileProcessor::fgStopRecording()
{
    StopRecordingMessage stopCmd;
    this->toBackgroundQueue.write_packet(sizeof(stopCmd), (uint8_t *)&stopCmd);
}

void Lv2AudioFileProcessor::fgSetLoopParameters(const std::string&fileName,const std::string &jsonLoopParameters)
{
    SetLoopParametersMessage cmd(++fgOperationId, fileName.c_str(),jsonLoopParameters.c_str());
    this->toBackgroundQueue.write_packet(cmd.size, (uint8_t *)&cmd);
}

void Lv2AudioFileProcessor::fgStopPlaying()
{
    StopPlaybackMessage cmd;
    this->toBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
}

void Lv2AudioFileProcessor::fgCuePlayback(const char *filename, size_t seekPos)
{
    ToobCuePlaybackMessage cmd{++fgOperationId, filename, seekPos};
    this->toBackgroundQueue.write_packet(cmd.size, (uint8_t *)&cmd);
}

void Lv2AudioFileProcessor::bgCloseTempFile()
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

void Lv2AudioFileProcessor::bgStartRecording(const char *filename, OutputFormat outputFormat)
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
void Lv2AudioFileProcessor::bgWriteBuffer(toob::AudioFileBuffer *buffer, size_t count)
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

void Lv2AudioFileProcessor::bgStopRecording()
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

        s << "/usr/bin/ffmpeg -y -f f32le -ar " << ((size_t)sampleRate) << " -ac " << (channels)
          << " -i " << fileToCmdline(bgTemporaryFile->Path())
          << " " << encodingArgs
          << " " << fileToCmdline(bgRecordingFilePath) << " 2>&1";

        execForOutput(s.str().c_str());
    }
    bgCloseTempFile();

    RecordingStoppedMessage cmd(this->bgRecordingFilePath.c_str());
    this->fromBackgroundQueue.write_packet(sizeof(cmd), (uint8_t *)&cmd);
}

void Lv2AudioFileProcessor::HandleMessages()
{
    if (loadRequested && activated)
    {
        loadRequested = false;
        CuePlayback();
    }
    size_t size = this->fromBackgroundQueue.peekSize();
    if (size == 0)
    {
        return;
    }
    char buffer[2048];
    if (size > sizeof(buffer))
    {
        OnFgError("Foreground buffer overflow");
        return;
    }
    size_t packetSize = fromBackgroundQueue.read_packet(sizeof(buffer), buffer);
    if (packetSize != 0)
    {
        BufferMessage *cmd = (BufferMessage *)buffer;
        switch (cmd->command)
        {
        case MessageType::RecordingStopped:
        {
            RecordingStoppedMessage *recordingStoppedCommand = (RecordingStoppedMessage *)cmd;
            OnFgRecordingStopped(recordingStoppedCommand->filename);
            break;
        }
        case MessageType::BackgroundError:
        {
            BackgroundErrorCommmand *errorCmd = (BackgroundErrorCommmand *)cmd;
            OnFgError(errorCmd->message);
            break;
        }
        case MessageType::Finished:
        {
            this->fgFinished = true;
            break;
        }
        case MessageType::UpdateLoopParameters:
        {
            UpdateLoopParametersCommand *updateCmd = (UpdateLoopParametersCommand *)cmd;
            if (updateCmd->operationId != fgOperationId)
            {
                return; // cancelled request.
            }
            OnFgUpdateLoopParameters(updateCmd->loopJson, updateCmd->seekPosSeconds, updateCmd->duration);
            break;
        }
        case MessageType::StartRecording:
        {
            ToobStartRecordingMessage *startCommand = (ToobStartRecordingMessage *)cmd;

            bgStartRecording(startCommand->filename, startCommand->outputFormat);
            break;
        }
        case MessageType::StopPlayback:
        {
            //StopPlaybackMessage *stopCommand = (StopPlaybackMessage *)cmd;
            fgStopPlaying();
            SetState(ProcessorState::Idle);
            break;
        }
        case MessageType::CuePlaybackResponse:
        {
            ToobCuePlaybackResponseMessage *responseCommand = (ToobCuePlaybackResponseMessage *)cmd;
            if (responseCommand->operationId != fgOperationId)
            {
                for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
                {
                    if (responseCommand->buffers[i])
                    {
                        this->bufferPool->PutBuffer(responseCommand->buffers[i]);
                    }
                }
                if (responseCommand->loopBuffer)
                {
                    fgDeleteLoopBuffer(responseCommand->loopBuffer);
                }
                return; // cancelled request.
            }

            OnFgCuePlaybackResponse(
                responseCommand->buffers,
                responseCommand->bufferCount,
                responseCommand->loopBuffer,
                responseCommand->loopParameters,
                responseCommand->seekPos,
                responseCommand->duration,
                responseCommand->loopParameterJson);
            break;
        }
        case MessageType::NextPlayBufferResponse:
        {
            ToobNextPlayBufferResponseMessage *responseCommand = (ToobNextPlayBufferResponseMessage *)cmd;
            if (responseCommand->operationId != fgOperationId)
            {
                this->bufferPool->PutBuffer(responseCommand->buffer);
                return;
            }

            OnFgNextPlayBufferResponse(responseCommand->operationId, responseCommand->buffer);
            break;
        }
        default:
            OnFgError("Unknown background message.");
        }
    }
}

AudioFileBuffer *Lv2AudioFileProcessor::bgReadDecoderBuffer()
{
    return bgReader.NextBuffer(this->bufferPool.get());
}

toob::AudioFileBuffer *BgFileReader::NextBuffer(
    toob::AudioFileBufferPool *bufferPool)
{

    using clock_t = std::chrono::high_resolution_clock;

    bufferPool->Reserve(PREROLL_BUFFERS + 1);

    toob::AudioFileBuffer *buffer = nullptr;
    if (!this->decoderStream && !useTestData)
    {
        // No decoder stream and no test data, so we can't read anything.
        return nullptr;
    }
    if (this->loopType == LoopType::BigLoop && this->readPos >= loopControlInfo.loopEnd_1)
    {
        // We have reached the end of the loop, so we need to reset the read position.
        this->readPos -= loopControlInfo.loopSize + loopControlInfo.loopEnd_1 - loopControlInfo.loopEnd_0;

        this->decoderStream.reset();

#ifndef NDEBUG
        auto start = clock_t::now();
#endif
        if (useTestData)
        {
            decoderStreamOpen(this->filePath, this->channels, this->sampleRate, this->readPos / (double)this->sampleRate);
        }
        else
        {
            if (this->readPos != this->lookaheadPosition) {
                throw std::logic_error("Read position does not match lookahead position.");
            }
            this->decoderStream = std::move(nextDecoderStream);
            // next decode stream will cue up asynchronously
            nextDecoderStream = std::make_unique<FfmpegDecoderStream>();
            this->lookaheadPosition = this->readPos;
            nextDecoderStream->open(this->filePath, this->channels, this->sampleRate, this->readPos / (double)this->sampleRate);
        }
// print the time taken to open the decoder stream.
#ifndef NDEBUG
        auto end = clock_t::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (duration > 500)
        {
            std::cerr << "Warning: Decoder stream open took " << duration << " ms for file: " << this->filePath << std::endl;
        }
#endif
    }

    try
    {
        size_t thisTime = bufferPool->GetBufferSize();
        if (this->loopType == LoopType::BigLoop)
        {
            if (this->readPos + thisTime >= loopControlInfo.loopEnd_1)
            {
                thisTime = (this->loopControlInfo.loopEnd_1 - readPos);
            }
        }
        else if (this->loopType == LoopType::BigStartSmallLoop)
        {
            if (readPos + thisTime >= loopControlInfo.loopStart)
            {
                thisTime = (this->loopControlInfo.loopStart - readPos);
            }
            if (thisTime == 0)
            {
                decoderStream.reset();
                useTestData = false;
                this->testdataL.clear();
                this->testdataR.clear();
                return nullptr; // no more streamed data.
            }
        }

        buffer = bufferPool->TakeBuffer();
        float *buffers[2];

        if (bufferPool->GetChannels() >= 2)
        {
            buffers[0] = buffer->GetChannel(0);
            buffers[1] = buffer->GetChannel(1);
        }
        else
        {
            buffers[0] = buffer->GetChannel(0);
            buffers[1] = nullptr; // mono file, so we can just read the data directly.
        }
        auto start = clock_t::now();

        size_t nRead = this->decoderStreamRead(buffers, thisTime);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - start).count();

        if (elapsed > 2000)
        {
            // xxx: DELETE ME.
            std::cerr << "Warning: Decoder stream read took " << elapsed << " ms for file: " << this->filePath << std::endl;
        }
        if (loopType == LoopType::BigLoop || loopType == LoopType::BigStartSmallLoop)
        {
            if (nRead < thisTime && nRead > 0)
            {
                // metatdata duration may not be accurate. supply missing samples if neccessary.
                for (size_t i = nRead; i < thisTime; ++i)
                {
                    buffers[0][i] = 0.0f;
                    if (buffers[1])
                    {
                        buffers[1][i] = 0.0f;
                    }
                }
                nRead = thisTime;
            }
        }
        if (nRead == 0)
        {
            bufferPool->PutBuffer(buffer);
            decoderStream.reset();
            return nullptr; // no more streamed data.
        }
        buffer->SetBufferSize(nRead);

        this->readPos += nRead;
        return buffer;
    }
    catch (const std::exception &e)
    {
        if (buffer)
        {
            bufferPool->PutBuffer(buffer);
        }
        decoderStream.reset();
        nextDecoderStream.reset();
        return nullptr;
    }
}

Lv2AudioFileProcessor::Lv2AudioFileProcessor(ILv2AudioFileProcessorHost *host, double sampleRate, int channels)
    : sampleRate(sampleRate), channels(channels), host(host)
{
    this->bufferPool = std::make_unique<toob::AudioFileBufferPool>(channels, (size_t)sampleRate / 10);
    this->filePath.reserve(1024);
    this->fgLoopParameterJson.reserve(1024);
    this->volumeDezipperL.SetSampleRate(sampleRate);
    this->volumeDezipperR.SetSampleRate(sampleRate);
    SetDbVolume(0.0, 0.0, true);
    size_t blendBufferSize = GetLoopBlendLength(sampleRate);
    bgReader.blendBufferL.resize(blendBufferSize * channels);
    bgReader.blendBufferR.resize(blendBufferSize);
}

static constexpr size_t FILE_LRU_MAX = 4;
static std::vector<std::string> fileLru;
static std::mutex fileLruMutex;

static bool ShouldPreCacheFile(const std::string &file)
{
    std::lock_guard lock{fileLruMutex};
    auto f = std::find(fileLru.begin(), fileLru.end(), file);
    if (f != fileLru.end())
    {
        return true;
    }
    else
    {
        fileLru.insert(fileLru.begin(), file);
        if (fileLru.size() >= FILE_LRU_MAX)
        {
            fileLru.pop_back();
        }
        return false;
    }
}

static void PreCacheFile(const std::filesystem::path &path)
{
    try
    {
        size_t fileSize = std::filesystem::file_size(path);
        size_t MAX_FILE_SIZE = 300 * 1024 * 1024;
        if (fileSize > MAX_FILE_SIZE)
        {
            return;
        }
        std::vector<uint8_t> buffer;
        buffer.resize(64 * 1024);

        FILE *f = fopen(path.c_str(), "r");
        if (!f)
        {
            return;
        }
        while (true)
        {
            size_t nRead = fread((void *)buffer.data(), sizeof(uint8_t), buffer.size(), f);
            if (nRead == 0)
            {
                break;
            }
        }
        fclose(f);
    }
    catch (const std::exception &e)
    {
    }
}

void Lv2AudioFileProcessor::bgUpdateForegroundLoopParameters(
    uint64_t operationId,
    const char*loopJson,
    double seekPosSeconds,
    double duration )
{
    if (operationId != fgOperationId)
    {
        // This is a cancelled request.
        return;
    }
    UpdateLoopParametersCommand cmd{
        operationId,
        loopJson,
        seekPosSeconds,
        duration
    };
    this->fromBackgroundQueue.write_packet(
        cmd.size,
        (uint8_t *)&cmd);
}

void  Lv2AudioFileProcessor::bgSetLoopParameters(uint64_t operationId, const char*fileName,const char *loopJson)
{
    this->bgOperationId = operationId;

    if (host) {
        host->bgSaveLoopJson(
            fileName,
            loopJson);
    }
    bgReader.loopParameterJson = loopJson;    
    bgCuePlayback(
        operationId,
        fileName,
        0,
        loopJson);
}
void Lv2AudioFileProcessor::bgCuePlayback(
    uint64_t operationId,
    const char *filename,
    size_t seekPos,
    const char *loopJson /* = nullptr*/)
{

    this->bgOperationId = operationId;
    if (bgOperationId != fgOperationId)
    {
        this->bgReader.filePath = filename;
        // This is a cancelled request.
        return;
    }

    double duration = 0;
    ToobPlayerSettings playerSettings;
    try
    {
        if (loopJson != nullptr)
        {
            bgReader.loopParameterJson = loopJson;
        }
        else if (this->host)
        {
            bgReader.loopParameterJson = this->host->bgGetLoopJson(filename);
        } else {
            bgReader.loopParameterJson = "";
        }
        if (!bgReader.loopParameterJson.empty())
        {
            try
            {
                std::stringstream ss(bgReader.loopParameterJson);
                pipedal::json_reader reader(ss);

                reader.read(&playerSettings);
            }
            catch (const std::exception &e)
            {
                std::stringstream ss;
                ss << "Failed to parse loop settings: " << e.what();
                throw std::runtime_error(ss.str());
            }
        } 
        // get file into memory cache in order to reduce dropouts while playing.

        if (bgReader.useTestData)
        {
            duration = bgReader.testdataL.size() / this->sampleRate;
        }
        else
        {
            if (ShouldPreCacheFile(filename))
            {
                PreCacheFile(filename);
            }
            duration = GetAudioFileDuration(filename);
        }

        // normalize the loop parameters.
        if (playerSettings.loopParameters_.loopEnable_)
        {
            if (playerSettings.loopParameters_.loopStart_ >= playerSettings.loopParameters_.loopEnd_)
            {
                playerSettings.loopParameters_.loopEnable_ = false;
            }
            else
            {
                if (playerSettings.loopParameters_.loopEnd_ > duration)
                {
                    playerSettings.loopParameters_.loopEnd_ = duration;
                }
                if (playerSettings.loopParameters_.start_ < 0)
                {
                    playerSettings.loopParameters_.loopEnable_ = false;
                }
                if (playerSettings.loopParameters_.start_ >= playerSettings.loopParameters_.loopEnd_)
                {
                    playerSettings.loopParameters_.start_ = playerSettings.loopParameters_.loopStart_;
                }
            }
        }
        // handle seek pos.
        if (seekPos == 0)
        {
            seekPos = (size_t)std::round(playerSettings.loopParameters_.start_ * this->sampleRate);
        }
        else
        {
            double dSeekPos = seekPos / this->sampleRate;
            if (playerSettings.loopParameters_.loopEnable_)
            {
                if (dSeekPos > duration)
                {
                    dSeekPos = duration;
                }
                if (dSeekPos >= playerSettings.loopParameters_.loopEnd_)
                {
                    dSeekPos = playerSettings.loopParameters_.loopStart_;
                }
                playerSettings.loopParameters_.start_ = dSeekPos;
                seekPos = (size_t)std::round(dSeekPos * this->sampleRate);
            }
        }
        bgReader.loopParameters = playerSettings.loopParameters_;
        bgReader.loopControlInfo = LoopControlInfo(
            bgReader.loopParameters,
            this->sampleRate,
            duration);

        double seekPosSeconds = seekPos / this->sampleRate;
        if (seekPosSeconds > duration)
        {
            seekPosSeconds = duration;
        }

        // interrim update of loop and play paramters to avoid unpleasant delay.
        bgUpdateForegroundLoopParameters(
            bgOperationId,
            bgReader.loopParameterJson.c_str(),
            seekPosSeconds,
            duration);

        bgReader.loopType = bgReader.loopControlInfo.loopType;
        auto loopType = bgReader.loopControlInfo.loopType;

        size_t startSample = (size_t)std::round(seekPosSeconds * this->sampleRate);
        ToobCuePlaybackResponseMessage responseCommand{
            operationId, 
            startSample, 
            bgReader.loopParameters, 
            duration, 
            bgReader.loopParameterJson.c_str()};

        if (loopType == LoopType::SmallLoop ||
            loopType == LoopType::BigStartSmallLoop)
        {
            AudioFileBuffer::ptr loopBuffer = bgReader.ReadLoopBuffer(
                filename,
                channels,
                sampleRate,
                bgReader.loopControlInfo);

            if (operationId != fgOperationId)
            {
                // This is a cancelled request.
                loopBuffer = nullptr;
                // return buffers to the pool.
                for (size_t i = 0; i < responseCommand.bufferCount; ++i)
                {
                    if (responseCommand.buffers[i])
                    {
                        bufferPool->PutBuffer(responseCommand.buffers[i]);
                    }
                }
                bgReader.Close();

                return;
            }

            responseCommand.loopBuffer = loopBuffer.detach();
        }

        if (loopType == LoopType::None ||
            loopType == LoopType::BigLoop ||
            loopType == LoopType::BigStartSmallLoop)
        {
            // generate pre-roll buffers.
            if (bgOperationId != fgOperationId)
            {
                // This is a cancelled request.
                return;
            }

            if (bgReader.decoderStream)
            {
                bgReader.decoderStream.reset();
            }
            bgReader.filePath = filename;
            bgReader.channels = channels;
            bgReader.sampleRate = sampleRate;
            bgReader.readPos = 0;

            bgReader.Init(filename, channels, duration, sampleRate, seekPosSeconds, playerSettings.loopParameters_, bufferPool->GetBufferSize());

            // If we are playing, then we need to prepare pre-roll buffers, so we have play-ahead buffering.

            for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
            {
                bool cancelled = operationId != fgOperationId;
                if (cancelled)
                {
                    responseCommand.buffers[i] = nullptr;
                }
                else
                {
                    responseCommand.buffers[i] = bgReadDecoderBuffer();
                }
                if (responseCommand.buffers[i])
                {
                    responseCommand.bufferCount++;
                }
            }
            // if we are playing a big loop, cue up the next decoder stream, so it can initialize asynchronously.
            if (loopType == LoopType::BigLoop)
            {
                bgReader.PrepareLookaheadDecoderStream();
            }

            bool cancelled = operationId != fgOperationId;

            if (cancelled)
            {
                // return buffers to the pool.
                for (size_t i = 0; i < PREROLL_BUFFERS; ++i)
                {
                    if (responseCommand.buffers[i])
                    {
                        bufferPool->PutBuffer(responseCommand.buffers[i]);
                    }
                }
                bgReader.Close();
                return;
            }
        }
        this->fromBackgroundQueue.write_packet(sizeof(responseCommand), (uint8_t *)&responseCommand);
    }
    catch (const std::exception &e)
    {
        bgError(e.what());
        return;
    }
}

void Lv2AudioFileProcessor::bgError(const char *message)
{
    bgReader.Close();
    BackgroundErrorCommmand errorCmd(message);
    this->fromBackgroundQueue.write_packet(sizeof(errorCmd), (uint8_t *)&errorCmd);
}

void Lv2AudioFileProcessor::bgStopPlaying()
{
    bgReader.Close();
}

void Lv2AudioFileProcessor::fgResetPlaybackQueue()
{
    if (fgLoopBuffer.Get() != nullptr)
    {
        fgDeleteLoopBuffer(fgLoopBuffer.detach());
    }
    while (!fgPlaybackQueue.empty())
    {
        bufferPool->PutBuffer(fgPlaybackQueue.pop_front());
    }
    fgPlaybackIndex = 0;
}

Lv2AudioFileProcessor::~Lv2AudioFileProcessor()
{
    Deactivate();
}

void Lv2AudioFileProcessor::OnUnderrunError()
{
#ifndef NDEBUG
    std::cout << "Audio file processor underrun error." << std::endl;
#endif

    OnFgError("Audio file processor underrun error.");
}
void Lv2AudioFileProcessor::OnFgError(const char *message)
{
    // if (this->host)
    // {
    //     this->host->LogProcessorError(message);
    // }
    fgResetPlaybackQueue();
    SetState(ProcessorState::Error);
}



void Lv2AudioFileProcessor::SetState(ProcessorState newState)
{
    if (state != newState)
    {
        state = newState;
        if (host)
        {
            host->OnProcessorStateChanged(state);
        }
    }
}

void Lv2AudioFileProcessor::OnFgNextPlayBufferResponse(uint64_t operationId, toob::AudioFileBuffer *buffer)
{
    if (operationId != fgOperationId)
    {
        // This is a cancelled request.
        if (buffer)
        {
            bufferPool->PutBuffer(buffer);
        }
        return;
    }
    if (buffer)
    {
        fgPlaybackQueue.push_back(buffer);
    }
}

LoopControlInfo::LoopControlInfo(const LoopParameters &loopParameters, double sampleRate, double duration)
{

    this->loopType = GetLoopType(loopParameters, sampleRate);
    this->start = (size_t)std::round(loopParameters.start_ * sampleRate);
    if (!loopParameters.loopEnable_)
    {
        loopEnd_0 = loopEnd_1 = std::numeric_limits<size_t>::max();
        return;
    }
    this->loopStart = (size_t)std::round(loopParameters.loopStart_ * sampleRate);
    this->loopEnd = (size_t)std::round(loopParameters.loopEnd_ * sampleRate);
    this->loopSize = loopEnd - loopStart;

    if (this->loopSize == 0)
    {
        // ugly corner case. Don't loop.
        this->loopType = LoopType::None;
        loopEnd_0 = loopEnd_1 = std::numeric_limits<size_t>::max();
        return;
    }
    size_t maxSample = (size_t)std::round(duration * sampleRate);

    size_t blendLength = GetLoopBlendLength(sampleRate);

    if (blendLength * 5 > loopSize)
    {
        this->loopEnd_0 = loopEnd;
        this->loopEnd_1 = loopEnd;
    }
    else if (loopStart >= blendLength)
    {
        // blend the last N samples before the end.
        this->loopEnd_0 = loopEnd - blendLength;
        this->loopEnd_1 = loopEnd;
    }
    else if (loopStart > blendLength / 2 + 1 && (this->loopEnd < maxSample - blendLength / 2 - 1))
    {
        // 1/2 and 1/2 across the loop
        this->loopEnd_0 = loopEnd - blendLength / 2;
        this->loopEnd_1 = loopEnd_0 + blendLength;
    }
    else if (this->loopEnd < maxSample - blendLength - 1)
    {
        // blend with data following loop end.
        this->loopEnd_0 = loopEnd;
        this->loopEnd_1 = loopEnd + blendLength;
    }
    else
    {
        // no blending.
        this->loopEnd_0 = loopEnd;
        this->loopEnd_1 = loopEnd;
    }
    if (this->loopType == LoopType::SmallLoop)
    {
        this->loopOffset = std::min(loopStart, start);
        // may need a little extra data at he beginning of the loop to perform blending.
        if (loopOffset < blendLength)
        {
            this->loopOffset = 0;
        }
        else
        {
            this->loopOffset -= blendLength;
        }
        this->loopBufferSize = loopEnd_1 - loopOffset;
    }
    else if (this->loopType == LoopType::BigStartSmallLoop)
    {
        this->loopOffset = loopStart; // for the loop only. start data is streamed.
        // may need a little extra data at he beginning of the loop to perform blending.
        if (loopOffset < blendLength)
        {
            this->loopOffset = 0;
        }
        else
        {
            this->loopOffset -= blendLength;
        }
        this->loopBufferSize = loopEnd_1 - loopOffset;
    }
    else
    {
        this->loopOffset = 0;
        this->loopBufferSize = maxSample;
    }
}

void Lv2AudioFileProcessor::OnFgUpdateLoopParameters(const char * loopJson, double seekPosSeconds,double duration)
{
    // Update the loop parameters in the background thread.
    this->playPosition = (size_t)std::round(seekPosSeconds * this->sampleRate);
    this->fgDuration = duration;
    if (host)
    {
        host->OnFgLoopJsonChanged(loopJson);
    }
}

void Lv2AudioFileProcessor::OnFgCuePlaybackResponse(
    AudioFileBuffer **buffers,
    size_t count,
    AudioFileBuffer *loopBuffer_,
    const LoopParameters &loopParameters,
    size_t seekPos,
    float duration,
    const char* loopParameterJson
)
{
    AudioFileBuffer::ptr loopBuffer;
    loopBuffer.attach(loopBuffer_);

    if (this->state == ProcessorState::CuePlayingThenPause ||
        this->state == ProcessorState::CuePlayingThenPlay ||
        this->state == ProcessorState::Playing)
    {
        this->fgLoopType = GetLoopType(loopParameters, this->sampleRate);

        fgResetPlaybackQueue();
        this->fgLoopBuffer = std::move(loopBuffer);
        this->fgLoopControlInfo = LoopControlInfo(loopParameters, this->sampleRate, duration);

        fgLoopParameters = loopParameters;
        fgLoopControlInfo = LoopControlInfo(loopParameters, this->sampleRate, duration);

        this->playPosition = seekPos;

        // Reset the playback queue and add the buffers.
        fgPlaybackIndex = 0;
        fgDuration = duration;
        this->fgLoopType = fgLoopControlInfo.loopType;

        for (size_t i = 0; i < count; ++i)
        {
            if (buffers[i])
            {
                fgPlaybackQueue.push_back(buffers[i]);
            }
        }
        if (this->state == ProcessorState::CuePlayingThenPause)
        {
            SetState(ProcessorState::Paused);
        }
        else
        {
            SetState(ProcessorState::Playing);
        }
        // deal with the corner case where blend buffers won't be filled on the first loop.
        if (fgLoopControlInfo.loopType == LoopType::BigLoop)
        {
            if (playPosition > fgLoopControlInfo.loopEnd_0 &&
                playPosition < fgLoopControlInfo.loopEnd_1)
            {
                bgReader.blendBufferL.resize(fgLoopControlInfo.loopEnd_1 - fgLoopControlInfo.loopEnd_0);
                bgReader.blendBufferR.resize(fgLoopControlInfo.loopEnd_1 - fgLoopControlInfo.loopEnd_0);
                for (size_t i = 0; i < bgReader.blendBufferL.size(); ++i)
                {
                    bgReader.blendBufferL[i] = 0.0f;
                    bgReader.blendBufferR[i] = 0.0f;
                }
            }
        }
        if (host)
        {
            host->OnFgLoopJsonChanged(loopParameterJson);
        }
    }
    else
    {
        // return them to the buffer pool.
        fgPlaybackIndex = 0;
        for (size_t i = 0; i < count; ++i)
        {
            if (buffers[i])
            {
                bufferPool->PutBuffer(buffers[i]);
            }
        }
    }
}

void Lv2AudioFileProcessor::OnFgRecordingStopped(const char *filename)
{
    SetState(ProcessorState::Idle);

    if (host)
    {
        host->OnProcessorRecordingComplete(filename);
    }
    CuePlayback(filename, 0, !playAfterRecording);
    playAfterRecording = false;
}
void Lv2AudioFileProcessor::Play(float *dst, size_t n_samples)
{
    if (this->state == ProcessorState::Playing)
    {
        if (this->fgLoopType == LoopType::None)
        {
            if (!this->fgPlaybackQueue.empty())
            {
                this->playPosition += n_samples;

                auto buffer = this->fgPlaybackQueue.front();
                float *playData = buffer->GetChannel(0);

                for (uint32_t i = 0; i < n_samples; ++i)
                {
                    dst[i] += playData[this->fgPlaybackIndex++] * volumeDezipperL.Tick();

                    ++this->playPosition;
                    if (fgPlaybackIndex == buffer->GetBufferSize())
                    {
                        fgPlaybackIndex = 0;
                        fgPlaybackQueue.pop_front();
                        bufferPool->PutBuffer(buffer);
                        if (fgPlaybackQueue.empty())
                        {
                            SetState(ProcessorState::Idle);
                            CuePlayback();
                            break;
                        }
                        buffer = fgPlaybackQueue.front();
                        playData = buffer->GetChannel(0);

                        fgRequestNextPlayBuffer();
                    }
                }
            }
        }
        else if (fgLoopType == LoopType::SmallLoop)
        {
            if (fgLoopBuffer.Get() != nullptr)
            {
                float *playData = fgLoopBuffer->GetChannel(0);

                for (uint32_t i = 0; i < n_samples; ++i)
                {
                    float value;
                    if (playPosition >= fgLoopControlInfo.loopEnd_0)
                    {
                        if (playPosition >= fgLoopControlInfo.loopEnd_1)
                        {
                            // loop point reached.
                            playPosition = playPosition - fgLoopControlInfo.loopSize;
                            value = playData[playPosition - fgLoopControlInfo.loopOffset];
                            if (playPosition >= fgLoopControlInfo.loopBufferSize - fgLoopControlInfo.loopOffset)
                            {
                                throw std::logic_error("Play position out of bounds.");
                            }
                        }
                        else
                        {
                            // blend data across the loop point
                            size_t blendIndex = playPosition - fgLoopControlInfo.loopSize;
                            float v0 = playData[blendIndex - fgLoopControlInfo.loopOffset];
                            float v1 = playData[playPosition - fgLoopControlInfo.loopOffset];
                            size_t t = playPosition - fgLoopControlInfo.loopEnd_0;
                            float blendFactor = (float)t / (float)(fgLoopControlInfo.loopEnd_1 - fgLoopControlInfo.loopEnd_0);
                            value = v0 * (1.0f - blendFactor) + v1 * blendFactor;
                        }
                    }
                    else
                    {
                        value = playData[playPosition - fgLoopControlInfo.loopOffset];
                    }
                    dst[i] += value * volumeDezipperL.Tick();
                    ++this->playPosition;
                }
            }
        }
        else if (fgLoopType == LoopType::BigLoop)
        {
            if (fgLoopBuffer.Get() != nullptr)
            {
                float *playData = fgLoopBuffer->GetChannel(0);

                for (uint32_t i = 0; i < n_samples; ++i)
                {
                    float value;
                    if (playPosition >= fgLoopControlInfo.loopEnd_1)
                    {
                        // loop point reached.
                        playPosition = playPosition - fgLoopControlInfo.loopSize;
                        value = playData[playPosition - fgLoopControlInfo.loopOffset];
                        if (playPosition >= fgLoopControlInfo.loopBufferSize - fgLoopControlInfo.loopOffset)
                        {
                            throw std::logic_error("Play position out of bounds.");
                        }
                    }
                    else
                    {
                        value = playData[playPosition - fgLoopControlInfo.loopOffset];
                    }
                    dst[i] += value * volumeDezipperL.Tick();
                    ++this->playPosition;
                }
            }
        }
    }
}

void Lv2AudioFileProcessor::Play(float *dstL, float *dstR, size_t n_samples)
{
    if (this->state == ProcessorState::Playing)
    {
        size_t ix = 0;
        while (ix < n_samples)
        {
            LoopType loopType = this->fgLoopType;
            if (loopType == LoopType::BigStartSmallLoop)
            {
                if (playPosition >= this->fgLoopControlInfo.loopStart)
                {
                    loopType = LoopType::SmallLoop;
                }
            }
            if (loopType == LoopType::None)
            {
                if (this->fgPlaybackQueue.empty())
                {
                    OnUnderrunError();
                    return;
                } else 
                {
                    auto buffer = this->fgPlaybackQueue.front();
                    float *playDataL = buffer->GetChannel(0);
                    float *playDataR = buffer->GetChannel(1);

                    for (; ix < n_samples; ++ix)
                    {
                        dstL[ix] += playDataL[this->fgPlaybackIndex] * volumeDezipperL.Tick();
                        dstR[ix] += playDataR[this->fgPlaybackIndex] * volumeDezipperR.Tick();
                        this->fgPlaybackIndex++;

                        ++this->playPosition;
                        if (fgPlaybackIndex == buffer->GetBufferSize())
                        {
                            fgPlaybackIndex = 0;
                            fgPlaybackQueue.pop_front();
                            bufferPool->PutBuffer(buffer);
                            if (fgPlaybackQueue.empty())
                            {
                                SetState(ProcessorState::Idle);
                                CuePlayback();
                                ix = n_samples;
                                break;
                            }
                            buffer = fgPlaybackQueue.front();
                            playDataL = buffer->GetChannel(0);
                            playDataR = buffer->GetChannel(1);

                            fgRequestNextPlayBuffer();
                        }
                    }
                }
            }
            else if (loopType == LoopType::SmallLoop)
            {
                if (fgLoopBuffer.Get() != nullptr)
                {
                    float *playDataL = fgLoopBuffer->GetChannel(0);
                    float *playDataR = fgLoopBuffer->GetChannel(1);

                    for (; ix < n_samples; ++ix)
                    {
                        float valueL, valueR;
                        if (playPosition >= fgLoopControlInfo.loopEnd_0)
                        {
                            if (playPosition >= fgLoopControlInfo.loopEnd_1)
                            {
                                /// loop point reached.
                                playPosition = playPosition - fgLoopControlInfo.loopSize;
                                if (playPosition - fgLoopControlInfo.loopOffset >= fgLoopControlInfo.loopBufferSize)
                                {
                                    throw std::logic_error("Play position out of bounds.");
                                }
                                valueL = playDataL[playPosition - fgLoopControlInfo.loopOffset];
                                valueR = playDataR[playPosition - fgLoopControlInfo.loopOffset];
                            }
                            else
                            {
                                // blend data across the loop point.
                                size_t blendIndex = playPosition - fgLoopControlInfo.loopEnd + fgLoopControlInfo.loopStart;
                                float blendFactor = (float)(playPosition - fgLoopControlInfo.loopEnd_0) / (float)(fgLoopControlInfo.loopEnd_1 - fgLoopControlInfo.loopEnd_0);

                                float v1L = playDataL[blendIndex - fgLoopControlInfo.loopOffset];
                                float v0L = playDataL[playPosition - fgLoopControlInfo.loopOffset];
                                valueL = v0L * (1.0f - blendFactor) + v1L * blendFactor;

                                float v1R = playDataR[blendIndex - fgLoopControlInfo.loopOffset];
                                float v0R = playDataR[playPosition - fgLoopControlInfo.loopOffset];
                                valueR = v0R * (1.0f - blendFactor) + v1R * blendFactor;
                            }
                        }
                        else
                        {
                            valueL = playDataL[playPosition - fgLoopControlInfo.loopOffset];
                            valueR = playDataR[playPosition - fgLoopControlInfo.loopOffset];
                        }
                        dstL[ix] += valueL * volumeDezipperL.Tick();
                        dstR[ix] += valueR * volumeDezipperR.Tick();
                        ++this->playPosition;
                    }
                }
            }
            else if (loopType == LoopType::BigLoop || loopType == LoopType::BigStartSmallLoop)
            {
                if (this->fgPlaybackQueue.empty())
                {
                    OnUnderrunError();
                    return;
                }
                auto buffer = this->fgPlaybackQueue.front();
                float *playDataL = buffer->GetChannel(0);
                float *playDataR = buffer->GetChannel(1);

                for (; ix < n_samples; ++ix)
                {
                    float vLeft, vRight;

                    if (playPosition == fgLoopControlInfo.loopStart && loopType == LoopType::BigStartSmallLoop)
                    {
                        break; // switch over to small loop processing.
                    }
                    if (fgPlaybackIndex == buffer->GetBufferSize())
                    {
                        fgPlaybackIndex = 0;
                        fgPlaybackQueue.pop_front();
                        bufferPool->PutBuffer(buffer);
                        if (fgPlaybackQueue.empty())
                        {
                            OnUnderrunError();
                            return;
                        }
                        buffer = fgPlaybackQueue.front();
                        playDataL = buffer->GetChannel(0);
                        playDataR = buffer->GetChannel(1);

                        fgRequestNextPlayBuffer();
                    }

                    if (playPosition >= fgLoopControlInfo.loopEnd_0)
                    {
                        if (playPosition >= fgLoopControlInfo.loopEnd_1)
                        {
                            // loop point reached.
                            playPosition = playPosition - fgLoopControlInfo.loopSize;
                            if (playPosition >= fgLoopControlInfo.loopEnd_0)
                            {
                                throw std::logic_error("Play position out of bounds.");
                            }
                            if (playPosition >= fgLoopControlInfo.loopEnd_0)
                            {
                                throw std::logic_error("Play position out of bounds.");
                            }

                            vLeft = playDataL[this->fgPlaybackIndex];
                            vRight = playDataR[this->fgPlaybackIndex];
                            this->fgPlaybackIndex++;
                        }
                        else
                        {
                            if (playPosition == fgLoopControlInfo.loopEnd_0)
                            {
                                // fill the blend buffers with the loop end data.
                                bgReader.blendBufferL.resize(0);
                                bgReader.blendBufferR.resize(0);
                                for (size_t j = fgLoopControlInfo.loopEnd_0; j < fgLoopControlInfo.loopEnd_1; ++j)
                                {
                                    bgReader.blendBufferL.push_back(playDataL[this->fgPlaybackIndex]);
                                    bgReader.blendBufferR.push_back(playDataR[this->fgPlaybackIndex]);
                                    ++fgPlaybackIndex;
                                    if (fgPlaybackIndex == buffer->GetBufferSize())
                                    {
                                        fgPlaybackIndex = 0;
                                        fgPlaybackQueue.pop_front();
                                        bufferPool->PutBuffer(buffer);
                                        if (fgPlaybackQueue.empty())
                                        {
                                            OnUnderrunError();
                                            return;
                                        }
                                        buffer = fgPlaybackQueue.front();
                                        playDataL = buffer->GetChannel(0);
                                        playDataR = buffer->GetChannel(1);

                                        fgRequestNextPlayBuffer();
                                    }
                                }
                            }
                            // blend data across the loop point.
                            size_t blendIndex = playPosition - fgLoopControlInfo.loopEnd_0;
                            float blendFactor = (float)(playPosition - fgLoopControlInfo.loopEnd_0) / (float)(fgLoopControlInfo.loopEnd_1 - fgLoopControlInfo.loopEnd_0);
                            float v0L = bgReader.blendBufferL[blendIndex];
                            float v1L = playDataL[this->fgPlaybackIndex];
                            vLeft = v0L * (1.0f - blendFactor) + v1L * blendFactor;
                            float v0R = bgReader.blendBufferR[blendIndex];
                            float v1R = playDataR[this->fgPlaybackIndex];
                            vRight = v0R * (1.0f - blendFactor) + v1R * blendFactor;

                            ++fgPlaybackIndex;
                        }
                    }
                    else
                    {
                        vLeft = playDataL[this->fgPlaybackIndex];
                        vRight = playDataR[this->fgPlaybackIndex];
                        this->fgPlaybackIndex++;
                    }

                    dstL[ix] += vLeft * volumeDezipperL.Tick();
                    dstR[ix] += vRight * volumeDezipperR.Tick();
                    ++this->playPosition;
                }
            }
        }
    }
}

void Lv2AudioFileProcessor::Record(const float *src, float level, size_t n_samples)
{
    if (this->state != ProcessorState::Recording)
    {
        return; // not recording.
    }
    if (this->realtimeRecordBuffer.Get() == nullptr)
    {
        this->realtimeRecordBuffer.attach(this->bufferPool->TakeBuffer());
        this->realtimeWriteIndex = 0;
    }
    this->playPosition += n_samples;
    float *buffer = this->realtimeRecordBuffer->GetChannel(0);

    for (size_t i = 0; i < n_samples; ++i)
    {
        auto value = src[i];
        value *= level;

        buffer[this->realtimeWriteIndex] = value;
        this->realtimeWriteIndex++;
        if (this->realtimeWriteIndex >= this->realtimeRecordBuffer->GetBufferSize())
        {
            SendBufferToBackground();

            this->realtimeRecordBuffer.attach(this->bufferPool->TakeBuffer());
            buffer = this->realtimeRecordBuffer->GetChannel(0);
            this->realtimeWriteIndex = 0;
        }
    }
}
void Lv2AudioFileProcessor::Record(const float *srcL, const float *srcR, float level, size_t n_samples)
{
    if (this->state != ProcessorState::Recording)
    {
        return; // not recording.
    }
    if (this->realtimeRecordBuffer.Get() == nullptr)
    {
        this->realtimeRecordBuffer.attach(this->bufferPool->TakeBuffer());
        this->realtimeWriteIndex = 0;
    }
    this->playPosition += n_samples;
    float *bufferL = this->realtimeRecordBuffer->GetChannel(0);
    float *bufferR = this->realtimeRecordBuffer->GetChannel(1);

    for (size_t i = 0; i < n_samples; ++i)
    {
        auto valueL = srcL[i] * level;
        auto valueR = srcR[i] * level;

        bufferL[this->realtimeWriteIndex] = valueL;
        bufferR[this->realtimeWriteIndex] = valueR;
        this->realtimeWriteIndex++;

        if (this->realtimeWriteIndex >= this->realtimeRecordBuffer->GetBufferSize())
        {
            SendBufferToBackground();

            this->realtimeRecordBuffer.attach(this->bufferPool->TakeBuffer());
            bufferL = this->realtimeRecordBuffer->GetChannel(0);
            bufferR = this->realtimeRecordBuffer->GetChannel(1);
            this->realtimeWriteIndex = 0;
        }
    }
}

void Lv2AudioFileProcessor::SetPath(const char *path)
{
    if (strcmp(path, filePath.c_str()) == 0)
    {
        return;
    }
    filePath = path;
    if (activated)
    {
        // If we are activated, then we need to reset the playback queue.
        fgResetPlaybackQueue();
    }
    else
    {
        loadRequested = true;
    }
}

void Lv2AudioFileProcessor::SetLoopParameters(const std::string& path,const std::string &jsonLoopParameters)
{
    if (activated)
    {
        fgStopPlaying();
        this->filePath = path;
        fgSetLoopParameters(path,jsonLoopParameters);
        SetState(ProcessorState::CuePlayingThenPause);
    }
    else
    {
        throw std::logic_error("Cannot set loop parameters when not activated.");
    }
}


void Lv2AudioFileProcessor::CuePlayback()
{
    CuePlayback(this->filePath.c_str(), 0, true);
}
void Lv2AudioFileProcessor::CuePlayback(
    const char *filename,
    size_t seekPos,
    bool pauseAftercue)
{
    loadRequested = false;
    if (strcmp(filename, this->filePath.c_str()) != 0)
    {
        this->filePath = filename;
    }

    if (this->state == ProcessorState::Playing || this->state == ProcessorState::Paused)
    {
        StopPlayback();
    }
    if (this->state == ProcessorState::StoppingRecording)
    {
        // Defer cueing until recording is stopped.
        return;
    }

    if (strlen(filename) == 0)
    {
        SetState(ProcessorState::Idle);
        return;
    }

    fgCuePlayback(filename, seekPos);
    if (pauseAftercue)
    {
        SetState(ProcessorState::CuePlayingThenPause);
    }
    else
    {
        SetState(ProcessorState::CuePlayingThenPlay);
    }
    this->playPosition = seekPos;
}

void Lv2AudioFileProcessor::TestCuePlayback(
    const char *filename,
    const char *loopParameterJson,
    size_t seekPos,
    bool pauseAftercue)
{
    loadRequested = false;
    if (strcmp(filename, this->filePath.c_str()) != 0)
    {
        this->filePath = filename;
    }
    if (strcmp(loopParameterJson, fgLoopParameterJson.c_str()) != 0)
    {
        fgLoopParameterJson = loopParameterJson;
    }

    if (this->state == ProcessorState::Playing || this->state == ProcessorState::Paused)
    {
        StopPlayback();
    }
    if (this->state == ProcessorState::StoppingRecording)
    {
        // Defer cueing until recording is stopped.
        return;
    }

    if (strlen(filename) == 0)
    {
        SetState(ProcessorState::Idle);
        return;
    }

    fgSetLoopParameters(filename, fgLoopParameterJson);

    if (pauseAftercue)
    {
        SetState(ProcessorState::CuePlayingThenPause);
    }
    else
    {
        SetState(ProcessorState::CuePlayingThenPlay);
    }
    this->playPosition = seekPos;
}


void Lv2AudioFileProcessor::SetDbVolume(float db, float pan, bool immediate)
{
    if (this->dbVolume == db && this->pan == pan && !immediate)
    {
        return;
    }
    this->dbVolume = db;
    this->pan = pan;

    if (pan < -1)
        pan = -1;
    if (pan > 1)
        pan = 1;
    if (dbVolume < -120)
        dbVolume = -120;

    auto af = LsNumerics::Db2Af(dbVolume, -120);
    float afLeft, afRight;
    if (pan < 0)
    {
        afLeft = af;
        afRight = af * (1.0f + pan);
    }
    else // if (pan > 0)
    {
        afRight = af;
        afLeft = af * (1.0f - pan);
    }
    float slew = immediate ? 0.0f : 0.1f; // 100ms slew time.
    volumeDezipperL.To(afLeft, slew);
    volumeDezipperR.To(afRight, slew);
}

void BgFileReader::PrepareLookaheadDecoderStream()
{
    if (loopControlInfo.loopType != LoopType::BigLoop) 
    {
        throw std::logic_error("PrepareLookaheadDecoderStream called with invalid loop type.");
    }
    // prepare the next stream, so that  it cues up asynchronously.
    if (!useTestData)
    {
        nextDecoderStream = std::make_unique<FfmpegDecoderStream>();
        this->lookaheadPosition = this->loopControlInfo.loopEnd_1 -
                this->loopControlInfo.loopSize -
                (this->loopControlInfo.loopEnd_1 - this->loopControlInfo.loopEnd_0);
        nextDecoderStream->open(
            this->filePath, 
            this->channels, 
            this->sampleRate,
            lookaheadPosition / sampleRate);
    }
}

void BgFileReader::Init(
    const std::filesystem::path &filename,
    int channels,
    double duration,
    double sampleRate,
    double seekPosSeconds,
    const LoopParameters &loopParameters_,
    size_t bufferSize)
{
    Close();

    this->filePath = filename;
    this->channels = channels;
    this->sampleRate = sampleRate;
    this->bufferSize = bufferSize;

    this->loopParameters = loopParameters_;
    this->loopControlInfo = LoopControlInfo(
        loopParameters,
        sampleRate,
        duration);
    ;

    loopType = GetLoopType(loopParameters, sampleRate);

    if (loopType == LoopType::None)
    {
        if (loopParameters.start_ > 0 && seekPosSeconds < loopParameters.start_)
        {
            // if the seek position is before the loop start, then we need to seek to the loop start.
            seekPosSeconds = loopParameters_.start_;
        }

        decoderStreamOpen(filename, channels, (uint32_t)sampleRate, seekPosSeconds);
    }
    else if (loopType == LoopType::SmallLoop)
    {
        throw std::logic_error("Shouldn't get here.");
    }
    else if (loopType == LoopType::BigLoop || loopType == LoopType::BigStartSmallLoop)
    {

        if (loopParameters.start_ > 0 && seekPosSeconds < loopParameters.start_)
        {
            // if the seek position is before the loop start, then we need to seek to the loop start.
            seekPosSeconds = loopParameters_.start_;
        }
        this->readPos = (size_t)std::round(seekPosSeconds * sampleRate);

        decoderStreamOpen(filename, channels, (uint32_t)sampleRate, seekPosSeconds);
    }
    else
    {
        throw std::runtime_error("Invalid loop type.");
    }
}

toob::AudioFileBuffer::ptr BgFileReader::ReadLoopBuffer(
    const std::string &filename,
    int channels,
    double sampleRate,
    const LoopControlInfo &loopControlInfo)
{
    size_t start = loopControlInfo.loopOffset;
    size_t end = loopControlInfo.loopOffset + loopControlInfo.loopBufferSize;

    AudioFileBuffer::ptr buffer = AudioFileBuffer::Create(channels, end - start);
    if (useTestData)
    {
        testReadIndex = start;
    }
    else
    {
        decoderStreamOpen(filename, channels, (uint32_t)sampleRate, start / sampleRate);
    }

    float *buffers[2];

    size_t length = end - start;
    if (channels >= 2)
    {
        buffers[0] = buffer->GetChannel(0);
        buffers[1] = buffer->GetChannel(1);
    }
    else
    {
        buffers[0] = buffer->GetChannel(0);
        buffers[1] = nullptr; // no second channel.
    }
    size_t nRead = decoderStreamRead(buffers, length);
    if (channels >= 2)
    {
        for (size_t i = nRead; i < length; ++i)
        {
            buffers[0][i] = 0;
            buffers[1][i] = 0;
        }
    }
    else
    {
        for (size_t i = nRead; i < length; ++i)
        {
            buffers[0][i] = 0;
        }
    }
    buffer->SetBufferSize(length);
    decoderStream = nullptr;
    return buffer;
}

void Lv2AudioFileProcessor::Pause()
{
    auto state = GetState();
    switch (state)
    {
    case ProcessorState::Playing:
        SetState(ProcessorState::Paused);
        break;
    case ProcessorState::Paused:
        break;
    case ProcessorState::CuePlayingThenPlay:
        SetState(ProcessorState::CuePlayingThenPause);
        break;
    case ProcessorState::CuePlayingThenPause:
        break;
    case ProcessorState::Recording:
        StopRecording();
        playAfterRecording = false;
        break;
    case ProcessorState::StoppingRecording:
        playAfterRecording = false;
        break;
    case ProcessorState::Idle:
        // Nothing to pause.
        break;
    case ProcessorState::Error:
        // Nothing to pause.
        break;
    default:
        throw std::logic_error("Invalid processor state for pause.");
    }
}
void Lv2AudioFileProcessor::Play()
{
    auto state = GetState();
    if (state == ProcessorState::Recording)
    {
        StopRecording();
        playAfterRecording = true;
        return;
    }
    if (state == ProcessorState::StoppingRecording)
    {
        playAfterRecording = true;
        return;
    }
    if (state == ProcessorState::CuePlayingThenPause)
    {
        SetState(ProcessorState::CuePlayingThenPlay);
    }
    if (state == ProcessorState::CuePlayingThenPlay)
    {
        SetState(ProcessorState::CuePlayingThenPause);
    }
    else if (state == ProcessorState::Playing)
    {
        this->SetState(ProcessorState::Paused);
    }
    else if (state == ProcessorState::Paused)
    {
        this->SetState(ProcessorState::Playing);
    }
    else if (state == ProcessorState::Idle || state == ProcessorState::Error)
    {
        CuePlayback();
    }
}

void BgFileReader::Close()
{
    if (this->decoderStream)
    {
        this->decoderStream->close();
        this->decoderStream.reset();
    }
}

void BgFileReader::Test_SetFileData(
    std::vector<float> &&testDataL,
    std::vector<float> &&testDataR)
{
    if (testDataL.size() != testDataR.size() && testDataR.size())
    {
        throw std::runtime_error("Test data left and right channels must be the same size.");
    }
    this->useTestData = true;
    this->testdataL = std::move(testDataL);
    this->testdataR = std::move(testDataR);
}
void BgFileReader::Test_SetFileData(
    const std::vector<float> &testDataL,
    const std::vector<float> &testDataR)
{
    if (testDataL.size() != testDataR.size() && testDataR.size())
    {
        throw std::runtime_error("Test data left and right channels must be the same size.");
    }
    this->useTestData = true;
    this->testdataL = (testDataL);
    this->testdataR = (testDataR);
}

size_t BgFileReader::decoderStreamRead(float **buffers, size_t n_frames)
{
    if (useTestData)
    {
        size_t nRead = 0;

        for (size_t i = 0; i < n_frames; ++i)
        {
            if (testReadIndex >= testdataL.size())
            {
                break; // no more data to read.
            }
            buffers[0][i] = testdataL[testReadIndex];
            if (buffers[1])
            {
                buffers[1][i] = testdataR[testReadIndex];
            }
            ++testReadIndex;
            ++nRead;
        }
        return nRead;
    }
    else
    {
        return decoderStream->read(buffers, n_frames);
    }
}

void BgFileReader::decoderStreamOpen(const std::filesystem::path &filePath, int channels, uint32_t sampleRate, double seekPosSeconds)
{
    if (useTestData)
    {
        size_t sampleOffset = (size_t)std::round(seekPosSeconds * sampleRate);
        this->testReadIndex = sampleOffset;
    }
    else
    {
        decoderStream = std::make_unique<FfmpegDecoderStream>();
        decoderStream->open(filePath, channels, sampleRate, seekPosSeconds);
    }
}

////////////////////////////

JSON_MAP_BEGIN(TimeSignature)
JSON_MAP_REFERENCE(TimeSignature, numerator)
JSON_MAP_REFERENCE(TimeSignature, denominator)
JSON_MAP_END()

JSON_MAP_BEGIN(Timebase)
JSON_MAP_REFERENCE(Timebase, units)
JSON_MAP_REFERENCE(Timebase, tempo)
JSON_MAP_REFERENCE(Timebase, timeSignature)
JSON_MAP_END()

JSON_MAP_BEGIN(LoopParameters)
JSON_MAP_REFERENCE(LoopParameters, start)
JSON_MAP_REFERENCE(LoopParameters, loopEnable)
JSON_MAP_REFERENCE(LoopParameters, loopStart)
JSON_MAP_REFERENCE(LoopParameters, loopEnd)
JSON_MAP_END()

JSON_MAP_BEGIN(ToobPlayerSettings)
JSON_MAP_REFERENCE(ToobPlayerSettings, timebase)
JSON_MAP_REFERENCE(ToobPlayerSettings, loopParameters)
JSON_MAP_END()
