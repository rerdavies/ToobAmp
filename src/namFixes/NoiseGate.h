/*
MIT License

Copyright (c) 2022 Steven Atkinson, 2023 Robin Davies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
//
//  NoiseGate.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 2/5/23.
//

#pragma once

#include <cmath>
#include <unordered_set>
#include <vector>

#include "NamDSP.h"

namespace dsp
{
namespace noise_gate
{
// Disclaimer: No one told me how noise gates work. I'm just going to try
// and have fun with it and see if I like what I get! :D

// "The noise floor." The loudness of anything quieter than this is bumped
// up to as if it were this loud for gating purposes (i.e. computing gain
// reduction).
constexpr double MINIMUM_LOUDNESS_DB = -120.0;
const double MINIMUM_LOUDNESS_POWER = pow(10.0, MINIMUM_LOUDNESS_DB / 10.0);

// Parts 2: The gain module.
// This applies the gain reduction taht was determined by the trigger.
// It's declared first so that the trigger can define listeners without a
// forward declaration.

// The class that applies the gain reductions calculated by a trigger instance.
#include "NamDSP.h"


class Gain : public NamDSP
{
public:
  using super = NamDSP;
  nam_float_t** Process(nam_float_t** inputs, const size_t numChannels, const size_t numFrames) override;

  void SetGainReduction(const std::vector<std::vector<nam_float_t>>& gainReduction)
  {
    this->pGainReduction  = &gainReduction;
  }

private:
  const std::vector<std::vector<nam_float_t>> *pGainReduction = nullptr;
};

// Part 1 of the noise gate: the trigger.
// This listens to a stream of incoming audio and determines how much gain
// to apply based on the loudness of the signal.

class TriggerParams
{
public:
  TriggerParams(const double time, const double threshold, const double ratio, const double openTime,
                const double holdTime, const double closeTime)
  : mTime(time)
  , mThreshold(threshold)
  , mRatio(ratio)
  , mOpenTime(openTime)
  , mHoldTime(holdTime)
  , mCloseTime(closeTime){};

  double GetTime() const { return this->mTime; };
  double GetThreshold() const { return this->mThreshold; };
  double GetRatio() const { return this->mRatio; };
  double GetOpenTime() const { return this->mOpenTime; };
  double GetHoldTime() const { return this->mHoldTime; };
  double GetCloseTime() const { return this->mCloseTime; };

private:
  // The time constant for quantifying the loudness of the signal.
  double mTime;
  // The threshold at which expanssion starts
  double mThreshold;
  // The compression ratio.
  double mRatio;
  // How long it takes to go from maximum gain reduction to zero.
  double mOpenTime;
  // How long to stay open before starting to close.
  double mHoldTime;
  // How long it takes to go from open to maximum gain reduction.
  double mCloseTime;
};

class Trigger : public NamDSP
{
public:
  using super = NamDSP;

  Trigger();

  nam_float_t** Process(nam_float_t** inputs, const size_t numChannels, const size_t numFrames) override;
  const std::vector<std::vector<nam_float_t>> &GetGainReduction() const { return this->mGainReduction; };
  void SetParams(const TriggerParams& params) { this->mParams = params; };
  void SetSampleRate(const double sampleRate) { this->mSampleRate = sampleRate; }

  void AddListener(Gain* gain)
  {
    // This might be risky dropping a raw pointer, but I don't think that the
    // gain would be destructed, so probably ok.
    this->mGainListeners.push_back(gain);
  }

  void PrepareBuffers(const size_t numChannels, const size_t maxFrames)
  {
    _PrepareBuffers(numChannels,maxFrames);
  }
 private:
  enum class State
  {
    MOVING = 0,
    HOLDING
  };

  double _GetGainReduction(const double levelDB) const
  {
    const double threshold = this->mParams.GetThreshold();
    // Quadratic gain reduction? :)
    return levelDB < threshold ? -(this->mParams.GetRatio()) * (levelDB - threshold) * (levelDB - threshold) : 0.0;
  }
  double _GetMaxGainReduction() const { return this->_GetGainReduction(MINIMUM_LOUDNESS_DB); }
  virtual void _PrepareBuffers(const size_t numChannels, const size_t numFrames) override;

  TriggerParams mParams;
  std::vector<State> mState; // One per channel
  std::vector<double> mLevel;

  // Hold the vectors of gain reduction for the block, in dB.
  // These can be given to the Gain object.
  std::vector<std::vector<nam_float_t>> mGainReduction;
  std::vector<double> mLastGainReductionDB;

  double mSampleRate;
  // How long we've been holding
  std::vector<double> mTimeHeld;

  std::vector<Gain*> mGainListeners;
};

}; // namespace noise_gate
}; // namespace dsp