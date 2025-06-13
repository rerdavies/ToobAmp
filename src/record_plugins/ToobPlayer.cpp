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

#include "lv2/atom/atom.h"
#include "ToobPlayer.hpp"
#include "../json.hpp"

using namespace pipedal;

static float SLOW_RATE = 0.15f;

ToobPlayer::ToobPlayer(double rate,
                       const char *bundle_path,
                       const LV2_Feature *const *features)
    : ToobPlayerBase(rate, bundle_path, features),
          lv2AudioFileProcessor(this, rate, 2)
{
    urids.atom__Path = MapURI(LV2_ATOM__Path);
    urids.atom__Float = MapURI(LV2_ATOM__Float);
    urids.atom__Double = MapURI(LV2_ATOM__Double);
    urids.atom__String = MapURI(LV2_ATOM__String);
    urids.player__seek_urid = MapURI("http://two-play.com/plugins/toob-player#seek");
    urids.player__loop_urid = MapURI("http://two-play.com/plugins/toob-player#loop");
    loopJson.reserve(1024);

    zipInL.SetSampleRate(rate);
    zipInR.SetSampleRate(rate);

    // set default values for loop parameters.
    {
        ToobPlayerSettings loopParams; // constructed with default values.
        std::stringstream ss;
        pipedal::json_writer writer(ss);
        writer.write(&loopParams);
        defaultLoopJson = ss.str();
        loopJson = defaultLoopJson;
        requestLoopJson = true; // request the loop json to be sent to the UI.
    }
}

ToobPlayer::~ToobPlayer()
{
}

static void applyPan(float pan, float vol, float &left, float &right)
{
    // hard pan law.
    if (pan < 0)
    {
        left = vol * 1.0f;
        right = vol * (1.0f + pan);
    }
    else
    {
        left = vol * (1.0 - pan);
        right = vol * 1.0f;
    }
}

void ToobPlayer::MuteVolume(float slewTime)
{
    lv2AudioFileProcessor.SetDbVolume(-120.0f,panFile.GetValue());
}
void ToobPlayer::HandleButtons()
{
    if (this->stop.IsTriggered())
    {
        lv2AudioFileProcessor.Stop();
        lv2AudioFileProcessor.CuePlayback();
    }
    if (this->pause.IsTriggered())
    {
        lv2AudioFileProcessor.Pause();
    }
    if (this->play.IsTriggered())
    {
        lv2AudioFileProcessor.Play();
    }
}

void ToobPlayer::Run(uint32_t n_samples)
{

    lv2AudioFileProcessor.HandleMessages();

    if (this->loadRequested)
    {
        this->loadRequested = false;
        CuePlayback(this->filePath.c_str(),loopJson.c_str(), requestedPlayPosition, true);
    }
    HandleButtons();

    const float *inL = this->inl.Get();
    const float *inR = this->inr.Get();
    float *outL = this->outl.Get();
    float *outR = this->outr.Get();


    // update the input mix.
    float il, ir;
    applyPan(this->panIn.GetValue(), this->volIn.GetAf(), il, ir);
    zipInL.To(il, SLOW_RATE);
    zipInR.To(ir, SLOW_RATE);
    
    if (volFile.HasChanged() || panFile.HasChanged())
    {
        lv2AudioFileProcessor.SetDbVolume(this->volFile.GetDb(), this->panFile.GetValue(), SLOW_RATE);
    }

    for (size_t i = 0; i < n_samples; ++i)
    {
        outL[i] = zipInL.Tick() * inL[i];
        outR[i] = zipInR.Tick() * inR[i];
    }

    lv2AudioFileProcessor.Play(outL,outR,n_samples);

    this->position.SetValue(lv2AudioFileProcessor.GetPlayPosition() / getRate(),n_samples );
    this->duration.SetValue(lv2AudioFileProcessor.GetDuration(), n_samples);
    this->state.SetValue((float)(int)GetState(), n_samples);

    if (requestLoopJson)
    {
        PutPatchPropertyString(0, urids.player__loop_urid, loopJson.c_str());
        requestLoopJson = false;
    }
}


void ToobPlayer::Activate()
{

    activated = true;
    super::Activate();
    lv2AudioFileProcessor.Activate();
    lv2AudioFileProcessor.SetDbVolume(volFile.GetDb(), panFile.GetValue(), true);

    float il, ir;
    applyPan(this->panIn.GetValue(), this->volIn.GetAf(), il, ir);
    zipInL.To(il, 0);
    zipInR.To(ir, 0);

    if (!this->filePath.empty())
    {
        this->loadRequested = true;
    }
}
void ToobPlayer::Deactivate()
{
    activated = false;
    lv2AudioFileProcessor.Deactivate();
    super::Deactivate();
}

void ToobPlayer::Seek(float value)
{
    position.SetValue(value); // set the output immediately (no n_samples parameter)
    auto state = GetState();
    bool isPlaying = (state == ProcessorState::Playing || state == ProcessorState::CuePlayingThenPlay);

    CuePlayback(this->filePath.c_str(), loopJson.c_str(), (size_t)value * getRate(),!isPlaying);
}
void ToobPlayer::OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *value)
{
    if (propertyUrid == this->urids.player__seek_urid)
    {
        if (value->type == this->urids.atom__Float)
        {
            const LV2_Atom_Float *floatValue = (const LV2_Atom_Float *)value;
            float value = floatValue->body;
            Seek(value);
        }
    }
    else if (propertyUrid == this->urids.player__loop_urid)
    {
        if (value->type == this->urids.atom__String)
        {
            // why do it this way instead of port values? becuase we need Double precision of the loop parameters! ffs!
            const LV2_Atom_String *string = (const LV2_Atom_String *)value;
            const char *body = static_cast<const char *>(LV2_ATOM_BODY_CONST(string));
            this->loopJson = body;
            requestLoopJson = true;
            requestedPlayPosition = 0;
            loadRequested = true; // re-cue the audio buffers.
        }
    }
    else
    {
        super::OnPatchSet(propertyUrid, value);
    }
}
bool ToobPlayer::OnPatchPathSet(LV2_URID propertyUrid, const char *value)
{
    if (propertyUrid == this->audioFile_urid)
    {
        SetFilePath(value);
        if (loopJson != defaultLoopJson) 
        {
            loopJson = defaultLoopJson;
            requestLoopJson = true; // request the loop json to be sent to the host.
        }

        SetFilePath(value);
        requestedPlayPosition = 0; // reset the play position.
        loadRequested = true;
        return true;
    }
    return false;
}

void ToobPlayer::OnPatchGet(LV2_URID propertyUrid)
{
    if (propertyUrid == urids.player__loop_urid)
    {
        requestLoopJson = true;
        return;
    }
    super::OnPatchGet(propertyUrid);
}
const char *ToobPlayer::OnGetPatchPropertyValue(LV2_URID propertyUrid)
{
    if (propertyUrid == this->audioFile_urid)
    {
        return this->filePath.c_str();
    }
    return nullptr;
}

void ToobPlayer::SetFilePath(const char *filename)
{
    if (strcmp(filename, this->filePath.c_str()) == 0)
        return;
    this->filePath = filename;
    if (activated)
    {
        this->PutPatchPropertyPath(0, this->audioFile_urid, filename);
    }
}



void ToobPlayer::CuePlayback()
{
    if (activated)
    {
        if (this->filePath.empty())
        {
            return;
        }
        CuePlayback(this->filePath.c_str(),loopJson.c_str(),0, true);
    }
}

void ToobPlayer::CuePlayback(const char *filename,const char*loopJson, size_t seekPos, bool pauseAfterLoad)
{

    if (activated)
    {
        SetFilePath(filename);

        lv2AudioFileProcessor.CuePlayback(filename,loopJson,seekPos,pauseAfterLoad);
    }
    else
    {
        SetFilePath(filename);
        this->requestedPlayPosition = seekPos;
        this->loadRequested = true;
    }
}

LV2_State_Status
ToobPlayer::OnRestoreLv2State(
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
                std::stringstream ss;
                ss << "ToobPlayer: LV2_State_Retrieve_Function returned unexpected type for audioFile_urid";
                LogError(ss.str());
                return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
            }
            modelFileName = MapFilename(features, (const char *)data, nullptr);
            RequestLoad(modelFileName.c_str());
        }
        else
        {
            modelFileName = "";
        }

        data = (*retrieve)(handle, this->urids.player__seek_urid, &size, &type, &flags);
        if (data)
        {
            if (type != this->urids.atom__Double)
            {
                std::stringstream ss;
                ss << "ToobPlayer: LV2_State_Retrieve_Function returned unexpected type for player__seek_urid";
                LogError(ss.str());
                return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
            }
            const double *doubleValue = (double*)data;
            this->requestedPlayPosition = (size_t)(*doubleValue * getRate());
            this->loadRequested = true;
        }
        else
        {
            this->requestedPlayPosition = 0;
        }

        data = (*retrieve)(handle, this->urids.player__loop_urid, &size, &type, &flags);
        if (data)
        {
            if (type != this->urids.atom__String)
            {
                std::stringstream ss;
                ss << "ToobPlayer: LV2_State_Retrieve_Function returned unexpected type for player__loop_urid";
                LogError(ss.str());
                return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
            }
            const char *body = static_cast<const char *>(data);
            this->loopJson = body;
            requestLoopJson = true;
            loadRequested = true;
        }
        else
        {
            this->loopJson = defaultLoopJson;
            requestLoopJson = true; // request the loop json to be sent to the host.
        }
        loadRequested = true; // request the file to be loaded.;
    }

    return LV2_State_Status::LV2_STATE_SUCCESS;
}

LV2_State_Status
ToobPlayer::OnSaveLv2State(
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

    store(handle, audioFile_urid, abstractPath.c_str(), abstractPath.length() + 1, urids.atom__Path, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

    const char *loopJsonData = loopJson.c_str();
    store(handle, urids.player__loop_urid, loopJsonData, loopJson.length() + 1, urids.atom__String, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

    return LV2_State_Status::LV2_STATE_SUCCESS;
}

std::string ToobPlayer::UnmapFilename(const LV2_Feature *const *features, const std::string &fileName)
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

std::string ToobPlayer::MapFilename(
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

void ToobPlayer::RequestLoad(const char *filename)
{
    this->filePath = filename;
    this->loadRequested = true;
}



void ToobPlayer::OnProcessorStateChanged(
    ProcessorState newState)
{
}
void ToobPlayer::LogProcessorError(const char *message)
{
}
void ToobPlayer::OnProcessorRecordingComplete(const char *fileName)
{
}

REGISTRATION_DECLARATION PluginRegistration<ToobPlayer> toobPlayerRegistration(ToobPlayer::URI);
