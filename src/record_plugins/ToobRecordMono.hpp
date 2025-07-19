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
#include "lv2ext/pipedal.lv2/ext/fileBrowser.h"
#include "ToobRecordMonoInfo.hpp"
#include "ToobRecordStereoInfo.hpp"
#include <queue>

#include "Lv2AudioFileProcessor.hpp"

using namespace lv2c::lv2_plugin;
using namespace record_plugin;
using namespace toob;

namespace toob
{
    class FfmpegDecoderStream;
};



class ToobRecordMono : public record_plugin::StereoRecordPluginBase, private toob::ILv2AudioFileProcessorHost
{
public:
    using super = record_plugin::StereoRecordPluginBase;

    static Lv2Plugin *Create(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    {
        return new ToobRecordMono(rate, bundle_path, features);
    }
    ToobRecordMono(double rate,
                   const char *bundle_path,
                   const LV2_Feature *const *features,
                   int channels = 1);

    virtual ~ToobRecordMono();

    static constexpr const char *URI = "http://two-play.com/plugins/toob-record-mono";

protected:
    Lv2AudioFileProcessor lv2AudioFileProcessor;

    virtual void OnProcessorStateChanged(
        ProcessorState newState) override;
    virtual void LogProcessorError(const char*message) override {
        super::LogError("%s", message); 
    }

    virtual std::string bgGetLoopJson(const std::string &filePath) override { return ""; }
    virtual void bgSaveLoopJson(const std::string &filePath, const std::string &loopJson) override {}
    virtual void OnFgLoopJsonChanged(const char*loopJson) override {}
  
    struct Urids
    {
        uint32_t atom__Path;
        uint32_t atom__String;
    };

    Urids urids;

    virtual void Mix(uint32_t n_samples);

    virtual void Run(uint32_t n_samples) override;

    virtual void Activate() override;
    virtual void Deactivate() override;

    virtual bool OnPatchPathSet(LV2_URID propertyUrid, const char *value) override;
    virtual const char *OnGetPatchPropertyValue(LV2_URID propertyUrid) override;

    LV2_State_Status
    OnRestoreLv2State(
        LV2_State_Retrieve_Function retrieve,
        LV2_State_Handle handle,
        uint32_t flags,
        const LV2_Feature *const *features);

    LV2_State_Status
    OnSaveLv2State(
        LV2_State_Store_Function store,
        LV2_State_Handle handle,
        uint32_t flags,
        const LV2_Feature *const *features);
    std::string UnmapFilename(const LV2_Feature *const *features, const std::string &fileName);
    std::string MapFilename(
        const LV2_Feature *const *features,
        const std::string &input,
        const char *browserPath);

    void RequestLoad(const char *filename);

protected:
    OutputFormat GetRecordFormat();

    bool isStereo = false;

    std::atomic<bool> loadRequested = false;

protected:
    int64_t errorBlinkSamples = 0;
    bool requestPutFilePath = false;
    ProcessorState GetState() const { return lv2AudioFileProcessor.GetState(); }
    void SetState(ProcessorState newState)
    {
        lv2AudioFileProcessor.SetState(newState);
    }   
    std::string RecordingFileExtension();

    const std::string& MakeNewRecordingFilename();
    void StopRecording();
    void StartRecording();
    void CuePlayback(const char *filename, size_t seekPos);
    void CuePlayback();
    void StopPlaying();
    void SetFilePath(const char *filename);
    void UpdateOutputControls(uint64_t sampleInFrame);
    void ResetPlayTime();

    virtual void OnProcessorRecordingComplete(const char *fileName) override;

    bool activated = false;


    const LV2_FileBrowser_Files *fileBrowserFilesFeature = nullptr;

    using clock_t = std::chrono::steady_clock;

    std::string filePath;
    std::string recordingFilePath;
    std::string recordingDirectory;

    size_t realtimeWriteIndex = 0;
};

class ToobRecordStereo : public ToobRecordMono
{
public:
    using super = ToobRecordMono;

    static Lv2Plugin *Create(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    {
        return new ToobRecordStereo(rate, bundle_path, features);
    }

    ToobRecordStereo(double rate,
                     const char *bundle_path,
                     const LV2_Feature *const *features);

    virtual ~ToobRecordStereo() {}
    static constexpr const char *URI = "http://two-play.com/plugins/toob-record-stereo";

protected:
    virtual void Mix(uint32_t n_samples) override;
};
