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
#include "lv2ext/pipedal.lv2/ext/fileBrowser.h"
#include "ToobRecordMonoInfo.hpp"


using namespace lv2c::lv2_plugin;
using namespace record_plugin;


class ToobRecordMono : public record_plugin::RecordPluginBase
{
public:
	using super = record_plugin::RecordPluginBase;

	static Lv2Plugin *Create(double rate,
							 const char *bundle_path,
							 const LV2_Feature *const *features)
	{
		return new ToobRecordMono(rate, bundle_path, features);
	}
	ToobRecordMono(double rate,
				 const char *bundle_path,
				 const LV2_Feature *const *features);

protected:
	virtual void Activate() override;
	virtual void Run(uint32_t n_samples) override;
	virtual void Deactivate() override;

	virtual bool OnPatchPathSet(LV2_URID propertyUrid,const char*value) override;
	virtual const char* OnGetPatchPropertyValue(LV2_URID propertyUrid) override;


private:
	void MakeNewRecordingFilename();
	void StopRecording();
	void SetFilePath(const char*filename);
	void UpdateOutputControls(uint64_t sampleInFrame);

	bool activated = false;
	enum class PluginState {
		Idle,
		Recording,
		Playing
	};

	const LV2_FileBrowser_Files* fileBrowserFilesFeature = nullptr;
	PluginState state = PluginState::Idle;

	using clock_t = std::chrono::steady_clock;

	clock_t::time_point startTime;
	float time_seconds = 0.0f;

	std::string filePath;
	std::string recordingFilePath; 
	std::string recordingDirectory;
};