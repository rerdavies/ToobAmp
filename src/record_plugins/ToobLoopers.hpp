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
#include <stddef.h>
#include <vector>
#include <filesystem>
#include <memory>
#include "lv2ext/pipedal.lv2/ext/fileBrowser.h"
#include "AudioFileBufferManager.hpp"
#include <thread>
#include "../TemporaryFile.hpp"
#include <queue>
#include "../Fifo.hpp"
#include "../ControlDezipper.h"


#include "ToobLooperFourInfo.hpp"
#include "ToobLooperOneInfo.hpp"


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

enum class TimeSig
{
	TwoTwo = 0,
	ThreeFour = 1,
	FourFour = 2,
	FiveFour = 3,
	SixEight = 4,
	SevenFour = 5
};



class ToobLooperEngine
{
protected:
	enum class LoopState
	{
		Idle,
		CueRecording,
		Recording,
		CueOverdub,
		Overdubbing,
		Playing,
		Stopping,
		Silent,
	};

	ToobLooperEngine(int channels, double sampleRate);


	void SetBeatLeds(RateLimitedOutputPort &bar_led, RateLimitedOutputPort&beat_led);
protected:
	bool isStereo = true;

protected:

	class ErrorBlinker
	{
	public:
		void Init(ToobLooperEngine *plugin) { this->plugin = plugin; }
		void SetError();
		bool HasError() { return hasError; }
		bool ErrorBlinkState();

	private:
	ToobLooperEngine *plugin = nullptr;
		bool hasError = false;
		uint64_t errorTime = 0;
	};

	class Loop
	{
	public:
		void Init(ToobLooperEngine *plugin);

		void fadeHead();
		void fadeTail();

		float getL(size_t index)
		{
			size_t bufferNumber = index / bufferSize;
			if (bufferNumber >= buffers.size())
				return 0.0f;
			auto buffer = buffers[bufferNumber];
			if (buffer == 0)
				return 0.0f;
			size_t bufferIndex = index % bufferSize;
			return buffer->GetChannel(0)[bufferIndex];
		}
		float getR(size_t index)
		{
			size_t bufferNumber = index / bufferSize;
			if (bufferNumber >= buffers.size())
				return 0.0f;
			auto buffer = buffers[bufferNumber];
			if (buffer == 0)
				return 0.0f;
			size_t bufferIndex = index % bufferSize;
			return buffer->GetChannel(1)[bufferIndex];
		}

		float &atL(size_t index)
		{
			size_t bufferNumber = index / bufferSize;
			if (bufferNumber >= buffers.size())
				buffers.resize(bufferNumber + 1);
			auto buffer = buffers[bufferNumber];
			if (buffer == 0)
			{
				buffer = plugin->bufferPool->TakeBuffer();
				buffers[bufferNumber] = buffer;
			}
			size_t bufferIndex = index % bufferSize;
			return buffer->GetChannel(0)[bufferIndex];
		}

		float &atR(size_t index)
		{
			size_t bufferNumber = index / bufferSize;
			if (bufferNumber >= buffers.size())
				buffers.resize(bufferNumber + 1);
			auto buffer = buffers[bufferNumber];
			if (buffer == 0)
			{
				buffer = plugin->bufferPool->TakeBuffer();
				buffers[bufferNumber] = buffer;
			}
			size_t bufferIndex = index % bufferSize;
			return buffer->GetChannel(1)[bufferIndex];
		}

		size_t declickSamples = 0;

		ToobLooperEngine *plugin = nullptr;
		LoopState state = LoopState::Idle;
		size_t sampleRate = 0;
		size_t bufferSize = 0;
		bool isMasterLoop = false;

		const size_t BUFFER_RESERVE = 60 * 10; // 60 seconds / 0.1second/buffer.
		std::vector<toob::AudioFileBuffer *> buffers{BUFFER_RESERVE};
		size_t length = 0;
		size_t master_loop_length = 0;

		size_t play_cursor = 0;
		size_t cue_samples = 0;
		size_t cue_start = 0;

		toob::ControlDezipper recordLevel;
		toob::ControlDezipper playbackLevel;

		ErrorBlinker recordError;
		ErrorBlinker playError;

		void CancelCue();

		void StopInner();

		void Record(ToobLooperEngine *plugin, size_t loopOffset);
		void Play(ToobLooperEngine *plugin, size_t loopOffset);
		void Stop(ToobLooperEngine *plugin, size_t loopOffset);

		void Reset();


		void ControlTap();
		void ControlLongPress();

		void ControlDown();
		void ControlUp();
		void ControlValue(bool value);

		bool lastControlValue = false;
		using clock_t = std::chrono::steady_clock;
		

		clock_t::time_point lastControlTime = clock_t::now() - std::chrono::milliseconds(10000);


		size_t CalculateCueSamples(size_t masterLoopOffset);

		void process(
			ToobLooperEngine *plugin,
			const float *__restrict inL,
			const float *__restrict inR,
			float *__restrict outL,
			float *__restrict outR, size_t n_samples);
	};

	// xxx;
	void Mix(
		uint32_t n_samples,
		const float *__restrict src,
		const float *__restrict srcR,
		float *__restrict dst,
		float *__restrict dstR);
	
	virtual float getTempo() = 0; 
	virtual TimeSig getTimesig() = 0;
	virtual double getOutputLevel() = 0;
	virtual bool getEnableRecordCountin() = 0;
	virtual size_t getNumberOfBars() = 0;
	virtual bool getRecordSyncOption() = 0;
	virtual bool GetRecordToOverdubOption() = 0;

	virtual void OnLoopEnd(Loop &loop) { }

	void UpdateLoopPosition(Loop &loop, RateLimitedOutputPort &position, size_t n_frames);
	void UpdateLoopLeds(Loop &loop, RateLimitedOutputPort &record_led, RateLimitedOutputPort &play_led);


	virtual void fgError(const char *message) = 0;

	double sampleRate = 44100.0;

	bool GetCountInBlink(Loop &loop);
	size_t GetCountInQuarterNotes();
	size_t GetSamplesPerQuarterNote();
	size_t GetSamplesPerBeat();



	std::vector<Loop> loops;

	bool finished = false;

	void SetMasterLoopLength(size_t size);

	uint64_t fastBlinkRate = 1;
	uint64_t slowBlinkRate = 1;

	bool has_time_zero = false;
	uint64_t time_zero = 0;
	uint64_t current_plugin_sample = 0;

	bool IsFixedLengthLoop();

	bool activated = false;
	enum class PluginState
	{
		Idle,
		Recording,
		CuePlaying,
		Playing,
		Error
	};

	std::shared_ptr<toob::AudioFileBufferPool> bufferPool;

	toob::ToobRingBuffer<false, true> toBackgroundQueue;
	toob::ToobRingBuffer<false, false> fromBackgroundQueue;

	std::unique_ptr<std::jthread> backgroundThread;

	void fgHandleMessages();


};

class ToobLooperOne : public record_plugin::ToobLooperOneBase, public ToobLooperEngine
{
public:
	using super = record_plugin::ToobLooperOneBase;

	static Lv2Plugin *Create(double rate,
		const char *bundle_path,
		const LV2_Feature *const *features)
	{
		return new ToobLooperOne(rate, bundle_path, features);
	}
	ToobLooperOne(
		double rate,
		const char *bundle_path,
		const LV2_Feature *const *features,
		int channels = 2);

	virtual ~ToobLooperOne();


protected:
	size_t activeLoops = 0;

	void PushLoop();
	void PopLoop();
	void ResetAll();
	void UndoLoop();

	virtual void Run(uint32_t n_samples) override;
	void HandleTriggers();
	void UpdateOutputControls(uint64_t sampleInFrame);

	void OnLoopEnd(Loop& loop) override;


	virtual void fgError(const char *message) override;

	virtual float getTempo() override { 
		return this->tempo.GetValue();
	}
	virtual TimeSig getTimesig() override { return (TimeSig)this->timesig.GetValue(); }
	virtual double getOutputLevel() override { return this->level.GetAf(); }
	virtual bool getEnableRecordCountin() override { return this->rec_count_in.GetValue() != 0; }	

	virtual size_t getNumberOfBars() override { return this->bars.GetValue(); }	
	virtual bool getRecordSyncOption() override { return this->rec_sync_option.GetValue(); }
	virtual bool GetRecordToOverdubOption() override;


	void UpdateLoopPosition(Loop&loop, RateLimitedOutputPort &position, size_t n_frames);
	void UpdateLoopLeds(Loop &loop, RateLimitedOutputPort &record_led, RateLimitedOutputPort &play_led);


	void OnSingleTap();
	void OnLongPress();
	void OnLongLongPress();
private:
	bool controlDown = false;
	enum class PluginState {
		Empty,
		CueRecording,
		Recording,
		Playing,
		CueOverdubbing,
		Overdubbing,
	};


	PluginState pluginState = PluginState::Empty;

	using clock_t = std::chrono::steady_clock;
	clock_t::time_point lastClickTime = clock_t::now()- std::chrono::milliseconds(10000);
	bool lastControlValue = false;
	size_t activeLoopsAtTap = 0;
};
class ToobLooperFour : public record_plugin::ToobLooperFourBase, public ToobLooperEngine
{
	static constexpr size_t N_LOOPS = 4;
public:

	using super = record_plugin::ToobLooperFourBase;

	static Lv2Plugin *Create(double rate,
							 const char *bundle_path,
							 const LV2_Feature *const *features)
	{
		return new ToobLooperFour(rate, bundle_path, features);
	}
	ToobLooperFour(double rate,
					 const char *bundle_path,
					 const LV2_Feature *const *features,
					 int channels = 2);

	virtual ~ToobLooperFour();

	virtual void Run(uint32_t n_samples) override;

	virtual void Activate() override;
	virtual void Deactivate() override;

	virtual void HandleTriggers();


	virtual void fgError(const char *message) override;

	virtual float getTempo() override { 
		return this->tempo.GetValue();
	}
	virtual TimeSig getTimesig() override { return (TimeSig)this->timesig.GetValue(); }
	virtual double getOutputLevel() override { return this->level.GetAf(); }
	virtual bool getEnableRecordCountin() override { return this->rec_count_in.GetValue() != 0; }	

	virtual size_t getNumberOfBars() override { return this->bars.GetValue(); }	
	virtual bool getRecordSyncOption() override { return this->rec_sync_option.GetValue(); }
	virtual bool GetRecordToOverdubOption() override;






	void UpdateOutputControls(uint64_t sampleInFrame);

};
