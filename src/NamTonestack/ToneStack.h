#pragma once

#include <string>
#include "RecursiveLinearFilter.h"

namespace tonestack_dsp
{
namespace tone_stack
{
class AbstractToneStack
{
public:
  AbstractToneStack() = default;
  virtual ~AbstractToneStack() = default;
  // Compute in the real-time loop
  virtual DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const int numChannels, const int numFrames) = 0;
  // Any preparation. Call from Reset() in the plugin
  virtual void Reset(const double sampleRate, const int maxBlockSize)
  {
    mSampleRate = sampleRate;
    mMaxBlockSize = maxBlockSize;
  };


  
protected:
  double GetSampleRate() const { return mSampleRate; };
  double mSampleRate = 0.0;
  int mMaxBlockSize = 0;
};

class BasicNamToneStack : public AbstractToneStack
{
public:
  enum class Param {
    Bass,
    Mid,
    Treble
  };

  BasicNamToneStack()
  {
    SetParam(Param::Bass, 5.0);
    SetParam(Param::Mid, 5.0);
    SetParam(Param::Treble, 5.0);
  };
  ~BasicNamToneStack() = default;

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const int numChannels, const int numFrames) override;
  void Reset(const double sampleRate, const int maxBlockSize) override;
  // :param val: Assumed to be between 0 and 10, 5 is "noon"

  void PrepareBuffers(size_t numChannels, size_t numFrames);
  
  void SetParam(Param param, const double val);

  double GetFrequencyResponse(float w);

protected:
  recursive_linear_filter::LowShelf mToneBass;
  recursive_linear_filter::Peaking mToneMid;
  recursive_linear_filter::HighShelf mToneTreble;

  // HACK not DRY w knob defs
  double mBassVal = 5.0;
  double mMiddleVal = 5.0;
  double mTrebleVal = 5.0;
};
}; // namespace tone_stack
}; // namespace tonestack_dsp
