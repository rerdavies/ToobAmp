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

// using namespace lv2c::lv2_plugin;


static REGISTRATION_DECLARATION  PluginRegistration<ToobRecordMono> registration(ToobRecordMono::URI);

constexpr char PREFERRED_PATH_SEPARATOR = std::filesystem::path::preferred_separator;



ToobRecordMono::ToobRecordMono(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : super(rate, bundle_path, features)
{

    // reserve space so that we can use the strings without allocating on the realtime thread (unless filenames are unreasonably long).
    filePath.reserve(1024); 
    recordingFilePath.reserve(1024);


    this->fileBrowserFilesFeature = this->GetFeature<LV2_FileBrowser_Files>(features,LV2_FILEBROWSER__files);

    if (this->fileBrowserFilesFeature) {
        // use the well-known directory for audio file recordings in PiPedal.
        char*cRecordingDirectory = fileBrowserFilesFeature->get_upload_path(
                fileBrowserFilesFeature->handle,
                "audiorecording");
        if (cRecordingDirectory) {
            this->recordingDirectory =  cRecordingDirectory;

            fileBrowserFilesFeature->free_path(
                fileBrowserFilesFeature->handle,
                cRecordingDirectory
            );
        }
    } 


    if (this->recordingDirectory.empty())
    {
        // Assume that we're running under a login. This is as good as any place to put them. 
        // At least we can write here (hopefully).
        this->recordingDirectory = "~/Music/Audio Recordings/";
    }
    if (!this->recordingDirectory.ends_with(PREFERRED_PATH_SEPARATOR))
    {
        recordingDirectory.append(1,PREFERRED_PATH_SEPARATOR);
    }
    filePath = ""; 
}


void generate_datetime_filename(char* buffer, size_t buffer_size, const char* prefix = "rec-") {
    // Get current time
    std::time_t now = std::time(nullptr);
    
    // Convert to local time
    std::tm* local_time = std::localtime(&now);
    
    // Ensure we don't overflow the buffer
    if (buffer_size < 32) {
        throw std::runtime_error("Buffer size is too small");
    }
    
    // Format the filename
    int written = std::snprintf(
        buffer, 
        buffer_size, 
        "%s%04d-%02d-%02d-%02d-%02d-%02d", 
        prefix,
        local_time->tm_year + 1900,  // years since 1900
        local_time->tm_mon + 1,      // months since January (1-12)
        local_time->tm_mday,         // day of the month (1-31)
        local_time->tm_hour,         // hours since midnight (0-23)
        local_time->tm_min,          // minutes after the hour (0-59)
        local_time->tm_sec           // seconds after the minute (0-60)
    );
    
    // verify if the entire string was written
    if (written < 0 || written >= static_cast<int>(buffer_size)) {
        throw std::runtime_error("Buffer size is too small");
    }
}

void ToobRecordMono::MakeNewRecordingFilename()
{
    this->recordingFilePath = this->recordingDirectory;

    char cFilename[256];
    generate_datetime_filename(cFilename, sizeof(cFilename),"rec-");

    // GCC: confirmed as zero-allocation by virtue of having reserved memory.
    // You may want to double check this on other compilers. There's always aaloc. 
    this->recordingFilePath += cFilename;

}

void ToobRecordMono::Activate()
{
    this->activated = true;
    super::Activate();
    this->state = PluginState::Idle;
}

void ToobRecordMono::UpdateOutputControls(size_t samplesInFrame)
{
    if (state == PluginState::Recording)
    {
        auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - this->startTime).count();
        time_seconds = time_ms*0.001f;
        this->record_time.SetValue(time_seconds,samplesInFrame); // throttled.

        bool ledBlinkStatus = ((time_ms / 300) & 1) == 0;
        this->record_led.SetValue(ledBlinkStatus? 1.0f: 0.0f);
        this->play_led.SetValue(0);
    } else if (state == PluginState::Playing)
    {
        auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - this->startTime).count();
        time_seconds = time_ms*0.001f;
        this->record_time.SetValue(time_seconds,samplesInFrame); // throttled.

        bool ledBlinkStatus = ((time_ms / 300) & 1) == 0;
        this->play_led.SetValue(ledBlinkStatus? 1.0f: 0.0f);
        this->record_led.SetValue(0);



    } else {
        this->play_led.SetValue(0);
        this->record_led.SetValue(0);
        this->record_time.SetValue(time_seconds);
    }
}

void ToobRecordMono::StopRecording()
{
    if (this->state == PluginState::Recording)
    {
        this->state = PluginState::Idle;

        SetFilePath(this->recordingFilePath.c_str());
    }
}
void ToobRecordMono::Run(uint32_t n_samples)
{
    auto src = in.Get();
    auto dst = out.Get();

    UpdateOutputControls(n_samples);

    if (this->stop.IsTriggered())
    {
        StopRecording();
        this->state = PluginState::Idle;
        UpdateOutputControls(0);
    }

    if (this->record.IsTriggered())
    {

        if (this->state == PluginState::Recording)
        {
            StopRecording();
            this->state = PluginState::Idle;
            UpdateOutputControls(0);
        } else {
            this->state = PluginState::Recording;

            this->startTime = clock_t::now();
            SetFilePath("");

            UpdateOutputControls(0);

            MakeNewRecordingFilename();

        }
    }
    if (this->play.IsTriggered())
    {
        if (this->state == PluginState::Recording)
        {
            StopRecording();
        }
        if (this->state == PluginState::Playing || this->filePath.length() == 0)
        {
            this->state = PluginState::Idle;
            UpdateOutputControls(0);
        } else {
            this->state = PluginState::Playing;
            this->startTime = clock_t::now();
            UpdateOutputControls(0);
        }

    }
    auto level = this->level.GetAf();

    for (uint32_t i = 0; i < n_samples; ++i)
    {
        auto value = src[i];
        dst[i] = value;
        this->level_vu.AddValue(value * level);
    }
}

void ToobRecordMono::Deactivate()
{
    this->activated = false;
    super::Deactivate();
}


bool ToobRecordMono::OnPatchPathSet(LV2_URID propertyUrid,const char*value) 
{
    if (propertyUrid == this->audioFile_urid)
    {
        this->filePath = value;
        return true;
    }
    return false;

}
const char* ToobRecordMono::OnGetPatchPropertyValue(LV2_URID propertyUrid)  {
    if (propertyUrid == this->audioFile_urid)
    {
        return this->filePath.c_str();
    }
    return nullptr;
}

void ToobRecordMono::SetFilePath(const char*filename)
{
    this->filePath = filename;
    if (activated)
    {
        this->PutPatchPropertyPath(0,this->audioFile_urid,filename);
    }
}

