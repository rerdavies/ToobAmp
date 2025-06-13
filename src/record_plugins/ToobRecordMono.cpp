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
#include <algorithm>
#include <cstdio>

// using namespace lv2c::lv2_plugin;

using namespace toob;

static REGISTRATION_DECLARATION PluginRegistration<ToobRecordMono> registration(ToobRecordMono::URI);

constexpr char PREFERRED_PATH_SEPARATOR = std::filesystem::path::preferred_separator;

ToobRecordMono::ToobRecordMono(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features,
    int channels)
    : super(rate, bundle_path, features),
      lv2AudioFileProcessor(this, rate, channels)
{

    urids.atom__Path = MapURI(LV2_ATOM__Path);
    urids.atom__String = MapURI(LV2_ATOM__String);

    // reserve space so that we can use the strings without allocating on the realtime thread (unless filenames are unreasonably long).
    filePath.reserve(1024);
    recordingFilePath.reserve(1024);
    recordingDirectory.reserve(1024);

    this->isStereo = channels > 1;

    this->recordingDirectory = "/tmp";

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
    {
        std::filesystem::path path = getenv("HOME");
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

const std::string &ToobRecordMono::MakeNewRecordingFilename()
{

    char cFilename[256];
    generate_datetime_filename(cFilename, sizeof(cFilename), RecordingFileExtension().c_str(), "rec-");

    // GCC: confirmed as zero-allocation by virtue of having reserved memory.
    // You may want to double check this on other compilers. There's always aaloc.
    return this->recordingFilePath = this->recordingDirectory + cFilename;
}
void ToobRecordMono::Activate()
{
    super::Activate();
    lv2AudioFileProcessor.Activate();

    this->activated = true;
}

void ToobRecordMono::ResetPlayTime()
{
}

void ToobRecordMono::OnProcessorStateChanged(
    ProcessorState newState)
{
    if (newState == ProcessorState::Error)
    {
        this->recordingFilePath.clear();
        this->filePath.clear();
        this->requestPutFilePath = true;
        this->errorBlinkSamples = (int64_t)(1.5 * getRate()); // 1 second of error blink.
    }
}

void ToobRecordMono::UpdateOutputControls(size_t samplesInFrame)
{
    uint64_t time_milliseconds = (uint64_t)(this->lv2AudioFileProcessor.GetPlayPosition() * 1000 / this->getRate());
    this->record_time.SetValue(time_milliseconds * 0.001f, samplesInFrame); // throttled.
    if (GetState() == ProcessorState::Recording)
    {

        bool ledBlinkStatus = ((time_milliseconds / 300) & 1) == 0;
        this->record_led.SetValue(ledBlinkStatus ? 1.0f : 0.0f);
        this->play_led.SetValue(0);
    }
    else if (GetState() == ProcessorState::Playing)
    {
        bool ledBlinkStatus = ((time_milliseconds / 300) & 1) == 0;
        this->play_led.SetValue(ledBlinkStatus ? 1.0f : 0.0f);
        this->record_led.SetValue(0);
    }
    else if (GetState() == ProcessorState::Error)
    {
        errorBlinkSamples -= samplesInFrame;
        if (this->errorBlinkSamples < 0)
        {
            SetState(ProcessorState::Idle);
            this->play_led.SetValue(0);
            this->record_led.SetValue(0);
        }
        else
        {
            int64_t t = (int64_t)(errorBlinkSamples * 1000) / (float)(getRate());
            bool fastBlinkStatus = ((t / 250) & 1) == 0;
            this->play_led.SetValue(fastBlinkStatus ? 1.0f : 0.0f);
            this->record_led.SetValue(fastBlinkStatus ? 1.0f : 0.0f);
        }
    }
    else
    {
        this->play_led.SetValue(0);
        this->record_led.SetValue(0);
    }
}

void ToobRecordMono::StartRecording()
{
    this->filePath = "";
    this->requestPutFilePath = true;
    lv2AudioFileProcessor.StartRecording(MakeNewRecordingFilename(), (toob::OutputFormat)(fformat.GetValue()));
}

void ToobRecordMono::StopRecording()
{
    lv2AudioFileProcessor.StopRecording();
}

void ToobRecordMono::Run(uint32_t n_samples)
{

    if (this->loadRequested)
    {
        this->loadRequested = false;
        if (!this->filePath.empty())
        {
            CuePlayback();
        }
        else
        {
            StopPlaying();
            StopRecording();
        }
    }

    UpdateOutputControls(n_samples);

    lv2AudioFileProcessor.HandleMessages();

    if (this->stop.IsTriggered())
    {
        if (this->GetState() == ProcessorState::Recording)
        {
            StopRecording();
        }
        else
        {
            StopPlaying();
            ResetPlayTime();
        }
        UpdateOutputControls(0);
    }

    if (this->record.IsTriggered())
    {

        if (this->GetState() == ProcessorState::Recording)
        {
            StopRecording();
            UpdateOutputControls(0);
        }
        else
        {
            StartRecording();
            UpdateOutputControls(0);
        }
    }
    if (this->play.IsTriggered())
    {
        switch (GetState())
        {
        case ProcessorState::Idle:
            lv2AudioFileProcessor.CuePlayback(this->filePath.c_str(), 0, false);
            break;
        case ProcessorState::StoppingRecording:
            lv2AudioFileProcessor.Play();
            break;
        case ProcessorState::CuePlayingThenPlay:
            // nothing.
            break;
        case ProcessorState::CuePlayingThenPause:
            SetState(ProcessorState::CuePlayingThenPlay);
            break;
        case ProcessorState::Playing:
            lv2AudioFileProcessor.CuePlayback(this->filePath.c_str(), 0, false);
            break;
        case ProcessorState::Paused:
            lv2AudioFileProcessor.Play();
            break;
        case ProcessorState::Recording:
            lv2AudioFileProcessor.Play();
            break;
        case ProcessorState::Error:
            // do nothing, wait for the error to be cleared.
            return;
        }
        UpdateOutputControls(0);
    }
    Mix(n_samples);

    if (this->requestPutFilePath)
    {
        this->requestPutFilePath = false;
        this->PutPatchPropertyPath(0, this->audioFile_urid, this->filePath.c_str());
    }
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

    auto state = lv2AudioFileProcessor.GetState();
    if (state == ProcessorState::Recording)
    {
        lv2AudioFileProcessor.Record(src, level, n_samples);
    }
    if (state == ProcessorState::Playing || state == ProcessorState::CuePlayingThenPlay)
    {
        /// mute thrue audio when playing back because we are "previewing"  the recording.
        for (size_t i = 0; i < n_samples; ++i)
        {
            dst[i] = 0; // mute thru audio.
        }
        lv2AudioFileProcessor.Play(dst, n_samples);
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
            std::max(std::abs(valueL), std::abs(valueR)) * level);
    }
    auto state = lv2AudioFileProcessor.GetState();
    if (state == ProcessorState::Recording)
    {
        lv2AudioFileProcessor.Record(srcL, srcR, level, n_samples);
    }
    if (state == ProcessorState::Playing || state == ProcessorState::CuePlayingThenPlay)
    {
        // We are "previewing" the recoding, so mute thru audio.
        // this is to avoid clicks when the playback starts.
        for (uint32_t i = 0; i < n_samples; ++i)
        {
            dstL[i] = 0;
            dstR[i] = 0;
        }
        lv2AudioFileProcessor.Play(dstL, dstR, n_samples);
    }
}

void ToobRecordMono::Deactivate()
{
    lv2AudioFileProcessor.Deactivate();
    this->activated = false;
    super::Deactivate();
}

bool ToobRecordMono::OnPatchPathSet(LV2_URID propertyUrid, const char *value)
{
    if (propertyUrid == this->audioFile_urid)
    {
        SetFilePath(value);
        this->CuePlayback(value, 0);
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
    else
    {
        // if not activated, we will load the file when activated.
        this->requestPutFilePath = true;
    }
}

void ToobRecordMono::StopPlaying()
{
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
        CuePlayback(this->filePath.c_str(), 0);
    }
}

void ToobRecordMono::CuePlayback(const char *filename, size_t seekPos)
{
    if (activated)
    {
        lv2AudioFileProcessor.CuePlayback(
            filename,
            seekPos, true);
    }
}

OutputFormat ToobRecordMono::GetRecordFormat()
{
    return (OutputFormat)fformat.GetValue();
}

std::string ToobRecordMono::RecordingFileExtension()
{
    switch (GetRecordFormat())
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

void ToobRecordMono::OnProcessorRecordingComplete(const char *filename)
{
    SetFilePath(filename);
}

ToobRecordStereo::ToobRecordStereo(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : super(rate, bundle_path, features, 2)
{
    this->isStereo = true;
}

ToobRecordMono::~ToobRecordMono()
{
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

    store(handle, audioFile_urid, abstractPath.c_str(), abstractPath.length() + 1, urids.atom__Path, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
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
