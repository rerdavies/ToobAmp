#include "ToneStack.h"

DSP_SAMPLE** tonestack_dsp::tone_stack::BasicNamToneStack::Process(DSP_SAMPLE** inputs, const int numChannels,
                                                         const int numFrames)
{
  DSP_SAMPLE** bassPointers = mToneBass.Process(inputs, numChannels, numFrames);
  DSP_SAMPLE** midPointers = mToneMid.Process(bassPointers, numChannels, numFrames);
  DSP_SAMPLE** treblePointers = mToneTreble.Process(midPointers, numChannels, numFrames);
  return treblePointers;
}

void tonestack_dsp::tone_stack::BasicNamToneStack::Reset(const double sampleRate, const int maxBlockSize)
{
  tonestack_dsp::tone_stack::AbstractToneStack::Reset(sampleRate, maxBlockSize);

  // Refresh the params!
  SetParam(Param::Bass, mBassVal);
  SetParam(Param::Mid, mMiddleVal);
  SetParam(Param::Treble, mTrebleVal);
}

void tonestack_dsp::tone_stack::BasicNamToneStack::SetParam(Param param, const double val)
{
  if (param == Param::Bass)
  {
    // HACK: Store for refresh
    mBassVal = val;
    const double sampleRate = GetSampleRate();
    const double bassGainDB = 4.0 * (val - 5.0); // +/- 20
    // Hey ChatGPT, the bass frequency is 150 Hz!
    const double bassFrequency = 150.0;
    const double bassQuality = 0.707;
    recursive_linear_filter::BiquadParams bassParams(sampleRate, bassFrequency, bassQuality, bassGainDB);
    mToneBass.SetParams(bassParams);
  }
  else if (param == Param::Mid)
  {
    // HACK: Store for refresh
    mMiddleVal = val;
    const double sampleRate = GetSampleRate();
    const double midGainDB = 3.0 * (val - 5.0); // +/- 15
    // Hey ChatGPT, the middle frequency is 425 Hz!
    const double midFrequency = 425.0;
    // Wider EQ on mid bump up to sound less honky.
    const double midQuality = midGainDB < 0.0 ? 1.5 : 0.7;
    recursive_linear_filter::BiquadParams midParams(sampleRate, midFrequency, midQuality, midGainDB);
    mToneMid.SetParams(midParams);
  }
  else if (param == Param::Treble)
  {
    // HACK: Store for refresh
    mTrebleVal = val;
    const double sampleRate = GetSampleRate();
    const double trebleGainDB = 2.0 * (val - 5.0); // +/- 10
    // Hey ChatGPT, the treble frequency is 1800 Hz!
    const double trebleFrequency = 1800.0;
    const double trebleQuality = 0.707;
    recursive_linear_filter::BiquadParams trebleParams(sampleRate, trebleFrequency, trebleQuality, trebleGainDB);
    mToneTreble.SetParams(trebleParams);
  }
}

double tonestack_dsp::tone_stack::BasicNamToneStack::GetFrequencyResponse(float frequency)
{
    double result = 1.0;
    result = this->mToneBass.GetFrequencyResponse(frequency);
    result *= this->mToneMid.GetFrequencyResponse(frequency);
    result *= this->mToneTreble.GetFrequencyResponse(frequency);
    return result;
}
