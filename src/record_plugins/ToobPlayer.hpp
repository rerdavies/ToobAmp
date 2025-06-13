// Copyright (c) 2025 Robin E. R. Davies
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
#include "ToobPlayerInfo.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include "Lv2AudioFileProcessor.hpp"
#include "lv2ext/pipedal.lv2/ext/fileBrowser.h"

#include "../ControlDezipper.h"

using namespace lv2c::lv2_plugin;
using namespace player_plugin;
using namespace toob;

class ToobPlayer : public player_plugin::ToobPlayerBase, private toob::ILv2AudioFileProcessorHost
{
public:
    using super = player_plugin::ToobPlayerBase;
    using ProcessorState = toob::ProcessorState;

    static Lv2Plugin *Create(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    {
        return new ToobPlayer(rate, bundle_path, features);
    }
    ToobPlayer(double rate,
               const char *bundle_path,
               const LV2_Feature *const *features);

    virtual ~ToobPlayer();

    static constexpr const char *URI = "http://two-play.com/plugins/toob-player";

protected:
    ProcessorState GetState() const 
    {
        return lv2AudioFileProcessor.GetState();
    }
    virtual void Run(uint32_t n_samples) override;

    virtual void Activate() override;
    virtual void Deactivate() override;

    virtual void OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *value) override;
    virtual bool OnPatchPathSet(LV2_URID propertyUrid, const char *value) override;
    virtual const char *OnGetPatchPropertyValue(LV2_URID propertyUrid) override;

    virtual void OnPatchGet(LV2_URID propertyUrid) override;

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

protected:
    Lv2AudioFileProcessor lv2AudioFileProcessor;

private:

    void OnProcessorStateChanged(
        ProcessorState newState) override;
    void LogProcessorError(const char *message) override;
    void OnProcessorRecordingComplete(const char *fileName) override;

    struct Urids
    {
        uint32_t atom__Path;
        uint32_t atom__String;
        uint32_t atom__Float;
        uint32_t atom__Double;
        uint32_t player__seek_urid;
        uint32_t player__loop_urid;
    };

    Urids urids;


    std::string loopJson;
    std::string defaultLoopJson;

    bool requestLoopJson = false;
    std::atomic<bool> loadRequested = false;
    size_t requestedPlayPosition = 0;

    void RequestLoad(const char *filename);

    void CuePlayback();
    void CuePlayback(const char *filename, const char*loopJson, size_t seekPos, bool pauseAfterLoad);
    void Seek(float value);

    void HandleButtons();
    void MuteVolume(float slewTime);


    // enum class PluginState
    // {
    //     // Must match PluginState values in PlayerControl.tsx
    //     Idle = 0,
    //     CuePlaying = 1,
    //     CuePlayPaused = 2,
    //     Pausing = 3,
    //     Paused = 4,
    //     Playing = 5,
    //     Error = 6
    // };

    
    std::string filePath;
    bool activated = false;
    size_t pausingDelay = 0;
    void SetFilePath(const char *filename);

    ControlDezipper zipInL, zipInR;


};
