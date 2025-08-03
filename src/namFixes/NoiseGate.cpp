/*
MIT License

Copyright (c) 2022 Steven Atkinson, 2023 Robin E. R. Davies

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
/**************************
  From https://github.com/sdatkinson/NeuralAmpModelerCore/
  
  - Convert to float buffers.
  - Provide a public method to pre-allocate buffers.

*************/

//
//  NoiseGate.cpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 2/5/23.
//

#include <cstring> // memcpy
#include <cmath> // pow
#include <sstream>
#include <algorithm>
#include "NoiseGate.h"

using namespace dsp;


static double _LevelToDB(double db)
{
  return 20.0 * log10(db);
}


dsp::noise_gate::Trigger::Trigger()
: mParams(0.05, -60.0, 1.5, 0.002, 0.050, 0.050)
, mSampleRate(0)
{
}


nam_float_t** dsp::noise_gate::Trigger::Process(nam_float_t** inputs, const size_t numChannels, const size_t numFrames)
{
  this->_PrepareBuffers(numChannels, numFrames);

  // A bunch of numbers we'll use a few times.
  const double alpha = pow(0.5, 1.0 / (this->mParams.GetTime() * this->mSampleRate));
  const double beta = 1.0 - alpha;
  const double threshold = this->mParams.GetThreshold();
  const double dt = 1.0 / this->mSampleRate;
  const double maxHold = this->mParams.GetHoldTime();
  const double maxGainReduction = this->_GetMaxGainReduction();
  // Amount of open or close in a sample: rate times time
  const double dOpen = -this->_GetMaxGainReduction() / this->mParams.GetOpenTime() * dt; // >0
  const double dClose = this->_GetMaxGainReduction() / this->mParams.GetCloseTime() * dt; // <0

  // The main algorithm: compute the gain reduction
  for (size_t c = 0; c < numChannels; c++)
  {
    for (size_t s = 0; s < numFrames; s++)
    {
      this->mLevel[c] =
        std::clamp(alpha * this->mLevel[c] + beta * (inputs[c][s] * inputs[c][s]), MINIMUM_LOUDNESS_POWER, 1000.0);
      const double levelDB = _LevelToDB(this->mLevel[c]);
      if (this->mState[c] == dsp::noise_gate::Trigger::State::HOLDING)
      {
        this->mGainReduction[c][s] = 1.0;
        this->mLastGainReductionDB[c] = 0.0;
        if (levelDB < threshold)
        {
          this->mTimeHeld[c] += dt;
          if (this->mTimeHeld[c] >= maxHold)
            this->mState[c] = dsp::noise_gate::Trigger::State::MOVING;
        }
        else
        {
          this->mTimeHeld[c] = 0.0;
        }
      }
      else
      { // Moving
        const double targetGainReduction = this->_GetGainReduction(levelDB);
        if (targetGainReduction > this->mLastGainReductionDB[c])
        {
          const double dGain = std::clamp(0.5 * (targetGainReduction - this->mLastGainReductionDB[c]), 0.0, dOpen);
          this->mLastGainReductionDB[c] += dGain;
          if (this->mLastGainReductionDB[c] >= 0.0)
          {
            this->mLastGainReductionDB[c] = 0.0;
            this->mState[c] = dsp::noise_gate::Trigger::State::HOLDING;
            this->mTimeHeld[c] = 0.0;
          }
        }
        else if (targetGainReduction < this->mLastGainReductionDB[c])
        {
          const double dGain = std::clamp(0.5 * (targetGainReduction - this->mLastGainReductionDB[c]), dClose, 0.0);
          this->mLastGainReductionDB[c] += dGain;
          if (this->mLastGainReductionDB[c] < maxGainReduction)
          {
            this->mLastGainReductionDB[c] = maxGainReduction;
          }
        }
        if (this->mLastGainReductionDB[c] < MINIMUM_LOUDNESS_DB)
        {
          this->mGainReduction[c][s] = 0; // avoid denorms.
        } else {
          this->mGainReduction[c][s] = std::pow((nam_float_t)10.0f,(nam_float_t)(this->mLastGainReductionDB[c])*((nam_float_t)0.05f));
        }
      }
    }
  }

  // Share the results with gain objects that are listening to this trigger:
  for (auto gain = this->mGainListeners.begin(); gain != this->mGainListeners.end(); ++gain)
    (*gain)->SetGainReduction(this->mGainReduction);

  // Copy input to output
  for (size_t c = 0; c < numChannels; c++)
    memcpy(this->mOutputs[c].data(), inputs[c], numFrames * sizeof(nam_float_t));
  return this->_GetPointers();
}

void dsp::noise_gate::Trigger::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const size_t oldChannels = this->_GetNumChannels();
  const size_t oldFrames = this->_GetNumFrames();

  const bool updateChannels = numChannels != oldChannels;
  const bool updateFrames = updateChannels || numFrames != oldFrames;

  if (updateChannels || updateFrames)
  {
    this->super::_PrepareBuffers(numChannels, numFrames);

    const double maxGainReduction = this->_GetMaxGainReduction();
    if (updateChannels || updateFrames)
    {
      if (updateChannels)
      {
        this->mGainReduction.resize(numChannels);
        this->mLastGainReductionDB.resize(numChannels);
        std::fill(this->mLastGainReductionDB.begin(), this->mLastGainReductionDB.end(), maxGainReduction);
        this->mState.resize(numChannels);
        std::fill(this->mState.begin(), this->mState.end(), dsp::noise_gate::Trigger::State::MOVING);
        this->mLevel.resize(numChannels);
        std::fill(this->mLevel.begin(), this->mLevel.end(), MINIMUM_LOUDNESS_POWER);
        this->mTimeHeld.resize(numChannels);
        std::fill(this->mTimeHeld.begin(), this->mTimeHeld.end(), 0.0);
      }
      for (auto& channel: mGainReduction)
      {
        channel.resize(numFrames);
      }
    }
  }
}

// Gain========================================================================

nam_float_t** dsp::noise_gate::Gain::Process(nam_float_t** inputs, const size_t numChannels, const size_t numFrames)
{
  // Assume that SetGainReductionDB() was just called to get data from a
  // trigger. Could use listeners...
  this->_PrepareBuffers(numChannels, numFrames);

  if (this->pGainReduction->size() != numChannels)
  {
    std::stringstream ss;
    ss << "Gain module expected to operate on " << this->pGainReduction->size() << "channels, but " << numChannels
       << " were provided.";
    throw std::runtime_error(ss.str());
  }
  if ((this->pGainReduction->size() == 0) && (numFrames > 0))
  {
    std::stringstream ss;
    ss << "No channels expected by gain module, yet " << numFrames << " were provided?";
    throw std::runtime_error(ss.str());
  }
  else if (this->pGainReduction->at(0).size() != numFrames)
  {
    std::stringstream ss;
    ss << "Gain module expected to operate on " << this->pGainReduction->at(0).size() << "frames, but " << numFrames
       << " were provided.";
    throw std::runtime_error(ss.str());
  }

  // Apply gain!
  for (size_t c = 0; c < numChannels; c++)
    for (size_t s = 0; s < numFrames; s++)
      this->mOutputs[c][s] = pGainReduction->at(c)[s] * inputs[c][s];

  return this->_GetPointers();
}
