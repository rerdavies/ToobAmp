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

#pragma once

#include "../json.hpp"
#include "AudioFileBufferManager.hpp"
#define NO_MLOCK
#include "ToobRingBuffer.hpp"
#include "../Fifo.hpp"
#include "../TemporaryFile.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include "FfmpegDecoderStream.hpp"
#include "../ControlDezipper.h"

class Lv2AudioFileProcessorTest;

namespace toob
{

    constexpr double PREROLL_TIME_SECONDS = 2.0;
    constexpr size_t PREROLL_BUFFERS = (size_t)(PREROLL_TIME_SECONDS / 0.1);


    enum class OutputFormat
    {
        Wav = 0,
        WavFloat = 1,
        Flac = 2,
        Mp3 = 3,
    };

    enum class TimebaseUnits
    {
        Seconds = 0,
        Samples = 1,
        Beats = 2,
    };

    class TimeSignature
    {
    public:
        uint32_t numerator_;
        uint32_t denominator_;

        DECLARE_JSON_MAP(TimeSignature);
    };

    class Timebase
    {
    public:
        uint32_t units_ = (uint32_t)TimebaseUnits::Seconds;
        double tempo_ = 120;
        TimeSignature timeSignature_{4, 4};

        bool isDefault() const
        {
            return units_ == (uint32_t)TimebaseUnits::Seconds && tempo_ == 120 && timeSignature_.numerator_ == 4 && timeSignature_.denominator_ == 4;
        }
        Timebase() = default;
        DECLARE_JSON_MAP(Timebase);
    };

    class LoopParameters
    {
        // double precision value are required!
    public:
        double start_ = 0;
        bool loopEnable_ = false;
        double loopStart_ = 0;
        double loopEnd_ = 0;

        bool isDefault() const
        {
            return start_ == 0 && !loopEnable_ && loopStart_ == 0 && loopEnd_ == 0;
        }

        DECLARE_JSON_MAP(LoopParameters);
    };
    class ToobPlayerSettings
    {
    public:
        Timebase timebase_;
        LoopParameters loopParameters_;
        DECLARE_JSON_MAP(ToobPlayerSettings);
    };

    enum class LoopType
    {
        None = 0,
        SmallLoop = 1, // one loop buffer.
        BigLoop = 2,  // streaming buffers
        BigStartSmallLoop = 3, // streaming buffers, then switch to a loop buffer.
    };

    LoopType GetLoopType(const LoopParameters &loopParameters, float sampleRate);

    enum class ProcessorState
    {
        // must match PiPedal PluginState in in ToobPlayerconrol.tsx
        Idle = 0,
        Recording = 1,
        StoppingRecording = 2,
        CuePlayingThenPlay = 3,
        CuePlayingThenPause = 4,
        Paused = 5,
        Playing = 6,
        Error = 7
    };

    struct LoopControlInfo
    {
        LoopControlInfo(const LoopParameters &loopParameters, double sampleRate, double duration);
        LoopControlInfo(LoopControlInfo &&v) = default;
        LoopControlInfo &operator=(LoopControlInfo &&v) = default;
        LoopControlInfo() = default;
        LoopControlInfo(const LoopControlInfo &) = default;
        LoopControlInfo &operator=(const LoopControlInfo &) = default;

        LoopType loopType = LoopType::None;
        size_t loopStart = 0;
        size_t loopEnd = 0;
        size_t loopOffset = 0;
        size_t loopBufferSize = 0;
        size_t start = 0;
        size_t loopSize = 0;
        size_t loopEnd_0 = 0; // start blending here.
        size_t loopEnd_1 = 0; // end blending here and loop.
    };

    class ILv2AudioFileProcessorHost
    {
        friend class ::Lv2AudioFileProcessorTest;

    public:
        virtual void LogProcessorError(const char *message) = 0;
        virtual void OnProcessorStateChanged(
            ProcessorState newState) = 0;

        virtual void OnProcessorRecordingComplete(
            const char *filePath) = 0;

        virtual void OnFgLoopJsonChanged(const char*loopJson) = 0;
        virtual std::string bgGetLoopJson(const std::string &filePath) = 0;
        virtual void bgSaveLoopJson(const std::string &filePath, const std::string &loopJson) = 0;
    };

    class BgFileReader
    {
        friend class ::Lv2AudioFileProcessorTest;

    public:
        BgFileReader() = default;
        ~BgFileReader() = default;

        void Init(
            const std::filesystem::path &filename,
            int channels,
            double duration,
            double sampleRate,
            double seekPosSeconds,
            const LoopParameters &loopParameters,
            size_t bufferSize);
        void Close();

        toob::AudioFileBuffer::ptr ReadLoopBuffer(
            const std::string &filename,
            int channels,
            double sampleRate,
            const LoopControlInfo &loopControlInfo);
        void PrepareLookaheadDecoderStream();
        toob::AudioFileBuffer *NextBuffer(
            toob::AudioFileBufferPool *bufferPool);


        void Test_SetFileData(
            std::vector<float> &&testDataL,
            std::vector<float> &&testDataR);
        void Test_SetFileData(
            const std::vector<float> &testDataL,
            const std::vector<float> &testDataR);

    public:
        std::filesystem::path filePath;
        std::string loopParameterJson;
        int channels = 0;
        double sampleRate = 0.0;
        LoopParameters loopParameters;
        LoopControlInfo loopControlInfo;
        size_t bufferSize = 0;

        double duration = 0.0;

        std::unique_ptr<toob::FfmpegDecoderStream> decoderStream;
        size_t lookaheadPosition = 0;
        std::unique_ptr<toob::FfmpegDecoderStream> nextDecoderStream;

        LoopType loopType = LoopType::None;
        size_t originalSeekPosForLoop = 0; // original seek position before looping.
        size_t readPos = 0;
        size_t operationId = 0;
        std::vector<float> blendBufferL;
        std::vector<float> blendBufferR;

        bool useTestData = false;
        size_t testReadIndex = std::numeric_limits<size_t>::max();
        std::vector<float> testdataL;
        std::vector<float> testdataR;

        void decoderStreamOpen(const std::filesystem::path &filePath, int channels, uint32_t sampleRate, double seekPosSeconds);
        size_t decoderStreamRead(float **buffers, size_t n_frames);
    };

    class Lv2AudioFileProcessor
    {
        friend class ::Lv2AudioFileProcessorTest;

    public:
        Lv2AudioFileProcessor(ILv2AudioFileProcessorHost *host, double samplerate, int channels);
        virtual ~Lv2AudioFileProcessor();

        ProcessorState GetState() const { return state; }
        void SetState(ProcessorState newState);

    private:

        std::string fgLoopParameterJson;
        static constexpr double PAUSE_TIME_SECONDS = 0.1;

        bool playAfterRecording = false;
        ProcessorState state = ProcessorState::Idle;

        ILv2AudioFileProcessorHost *host = nullptr;

        LoopType fgLoopType = LoopType::None;

        toob::Fifo<toob::AudioFileBuffer *, PREROLL_BUFFERS*2> fgPlaybackQueue;

        toob::AudioFileBuffer::ptr fgLoopBuffer;

        LoopControlInfo fgLoopControlInfo;

        bool fgFinished = false;

        double sampleRate;
        int channels;
        toob::ToobRingBuffer<false, true> toBackgroundQueue;
        toob::ToobRingBuffer<false, false> fromBackgroundQueue;

        std::unique_ptr<std::jthread> backgroundThread;

        std::filesystem::path bgRecordingFilePath;
        std::unique_ptr<pipedal::TemporaryFile> bgTemporaryFile;
        FILE *bgFile = nullptr;
        OutputFormat bgOutputFormat;

    public:
        void Play();
        void Stop();
        void Pause();
        void StartRecording(
            const std::string &recordingFilePath,
            OutputFormat recordFormat);
        void StopRecording();
        void StopPlayback();
        void HandleMessages();

        void SetLoopParameters(const std::string& path,const std::string &jsonLoopParameters);
        void CuePlayback();

        void CuePlayback(
            const char *filename,
            size_t seekPos = 0,
            bool pauseAfterCue = true);

        // Test only. Atomically set both.
        void TestCuePlayback(
            const char *filename,
            const char*loopJson,
            size_t seekPos = 0,
            bool pauseAfterCue = true);

        void Record(const float *src, float level, size_t n_samples);
        void Record(const float *srcL, const float *srcR, float level, size_t n_samples);
        void Play(float *dst, size_t n_samples);
        void Play(float *dstL, float *dstR, size_t n_samples);

        const std::string &GetPath() const { return filePath; }
        void SetPath(const char *path);
        void SetDbVolume(float db, float pan = 0, bool immediate = false);
        float GetDbVolume() const { return dbVolume; };
        float GetPan() const { return pan; }

    private:
        float dbVolume = 0.0f;
        float pan = 0.0f;
        toob::ControlDezipper volumeDezipperL;
        toob::ControlDezipper volumeDezipperR;

        std::string filePath;

        void SendBufferToBackground();

        void fgSetLoopParameters(const std::string&fileName,const std::string &jsonLoopParameters);
        void fgStartRecording(const std::string &recordingFilePath, OutputFormat recordFormat);
        void fgRecordBuffer(toob::AudioFileBuffer *buffer, size_t count);
        void fgStopRecording();
        void fgDeleteLoopBuffer(toob::AudioFileBuffer *buffer);

        void fgCuePlayback(
            const char *filename,
            size_t seekPos);

        void fgRequestNextPlayBuffer();
        void fgStopPlaying();

    private:
        void OnFgError(const char *message);
        void OnUnderrunError();

        void OnFgCuePlaybackResponse(
            toob::AudioFileBuffer **buffers,
            size_t count,
            toob::AudioFileBuffer *loopBuffer,
            const LoopParameters &loopParameters,
            size_t position,
            float duration,
            const char*loopJson
        );

        void OnFgUpdateLoopParameters(const char *loopJson, double seekPosSeconds, double duration);

        void OnFgNextPlayBufferResponse(size_t operationId, toob::AudioFileBuffer *buffer);

        void fgResetPlaybackQueue();

        virtual void OnFgRecordingStopped(const char *fileName);

        size_t fgPlaybackIndex = 0;
        LoopParameters fgLoopParameters;
        double fgDuration = 0.0;
        size_t playPosition = 0;

    public:
        double GetDuration() const { return fgDuration; }
        size_t GetPlayPosition() const { return playPosition; }
        void Activate();
        void Deactivate();

    private:
        bool activated = false;
        bool loadRequested = true;

        std::atomic<uint64_t> fgOperationId = 0;
        uint64_t bgOperationId = 0;
        void bgCloseTempFile();
        void bgStartRecording(const char *filename, OutputFormat outputFormat);
        void bgWriteBuffer(toob::AudioFileBuffer *buffer, size_t count);
        void bgStopRecording();

        void bgUpdateForegroundLoopParameters(
            uint64_t operationId,
            const char* loopJson,
            double seekPosSeconds,
            double duration);


        void bgSetLoopParameters(uint64_t operationId, const char* fileName,const char *loopJson);
        void bgCuePlayback(uint64_t operationId, const char *filename, size_t seekPos, const char *loopJson = nullptr);
        void bgStopPlaying();
        void bgError(const char *message);

        std::shared_ptr<toob::AudioFileBufferPool> bufferPool;
        toob::AudioFileBuffer::ptr realtimeRecordBuffer;
        size_t realtimeWriteIndex = 0;

        toob::AudioFileBuffer *bgReadDecoderBuffer();

    public:
        BgFileReader bgReader;
    };

    size_t GetLoopBlendLength(double sampleRate);

}