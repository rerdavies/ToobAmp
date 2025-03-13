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
#include "AudioFileBufferManager.hpp"
#include <thread>
#include "../TemporaryFile.hpp"
#include <queue>
#include "../Fifo.hpp"

#define NO_MLOCK
#include "ToobRingBuffer.hpp"

using namespace lv2c::lv2_plugin;
using namespace record_plugin;

namespace toob
{
	class AudioFileBufferPool;
	class FfmpegDecoderStream;
};

enum class OutputFormat
{
	Wav = 0,
	WavFloat = 1,
	Flac = 2,
	Mp3 = 3,
};

class ToobRecordMono : public record_plugin::StereoRecordPluginBase
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
	virtual OutputFormat GetOutputFormat();

	bool isStereo = false;

	bool loadRequested = false;

protected:
	size_t playPosition = 0;
	bool finished = false;
	std::string RecordingFileExtension();
	void SendBufferToBackground();

	void MakeNewRecordingFilename();
	void StopRecording();
	void StartRecording();
	void CuePlayback(const char *filename);
	void CuePlayback();
	void StopPlaying();
	void SetFilePath(const char *filename);
	void UpdateOutputControls(uint64_t sampleInFrame);
	void ResetPlayTime();

	bool activated = false;
	enum class PluginState
	{
		Idle,
		Recording,
		CuePlaying,
		Playing,
		Error
	};

	const LV2_FileBrowser_Files *fileBrowserFilesFeature = nullptr;
	PluginState state = PluginState::Idle;

	using clock_t = std::chrono::steady_clock;

	std::string filePath;
	std::string recordingFilePath;
	std::string recordingDirectory;
	std::shared_ptr<toob::AudioFileBufferPool> bufferPool;

	toob::AudioFileBuffer::ptr realtimeBuffer;
	size_t realtimeWriteIndex = 0;

	toob::ToobRingBuffer<false, true> toBackgroundQueue;
	toob::ToobRingBuffer<false, false> fromBackgroundQueue;

	std::unique_ptr<std::jthread> backgroundThread;

	std::filesystem::path bgRecordingFilePath;
	std::unique_ptr<pipedal::TemporaryFile> bgTemporaryFile;
	FILE *bgFile = nullptr;
	OutputFormat bgOutputFormat;

	void fgHandleMessages();
	void fgError(const char *message);

	void bgCloseTempFile();
	void bgStartRecording(const char *filename, OutputFormat outputFormat);
	void bgWriteBuffer(toob::AudioFileBuffer *buffer, size_t count);
	void bgStopRecording();

	void bgStopPlaying();

	std::unique_ptr<toob::FfmpegDecoderStream> decoderStream;

	toob::AudioFileBuffer *bgReadDecoderBuffer();

	void fgResetPlaybackQueue();

	void bgCuePlayback(const char *filename);

	size_t fgPlaybackIndex;
	toob::Fifo<toob::AudioFileBuffer *, 16> fgPlaybackQueue;
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
