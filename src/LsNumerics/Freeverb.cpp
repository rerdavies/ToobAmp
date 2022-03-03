/*
Copyright (c) 1995-2021 Perry R. Cook and Gary P. Scavone

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

Any person wishing to distribute modifications to the Software is
asked to send the modifications to the original developer so that they
can be incorporated into the canonical version.  This is, however, not
a binding provision of this license.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
   Ported from STK source by Robin Davies.
   Copyright (c) 2022 Robin Davies
   Changes licensed under the terms of the original STK license.
    
   https://github.com/thestk/stk
*/

/***********************************************************************/
/*! \class Freeverb
    \brief Jezar at Dreampoint's Freeverb, implemented in STK.

    Freeverb is a free and open-source Schroeder reverberator
    originally implemented in C++. The parameters of the reverberation
    model are exceptionally well tuned. Freeverb uses 8
    lowpass-feedback-comb-filters in parallel, followed by 4 Schroeder
    allpass filters in series.  The input signal can be either mono or
    stereo, and the output signal is stereo.  The delay lengths are
    optimized for a sample rate of 44100 Hz.

    Ported to STK by Gregory Burlet, 2012.
*/
/***********************************************************************/

#include "Freeverb.hpp"
#include <cmath>
#include <iostream>

using namespace LsNumerics;
using namespace stk;

// Set static delay line lengths
const StkFloat Freeverb::fixedGain = 0.015;
const StkFloat Freeverb::scaleWet = 3;
const StkFloat Freeverb::scaleDry = 2;
const StkFloat Freeverb::scaleDamp = 0.4;
const StkFloat Freeverb::scaleRoom = 0.28;
const StkFloat Freeverb::offsetRoom = 0.7;
int Freeverb::cDelayLengths[] = {1617, 1557, 1491, 1422, 1356, 1277, 1188, 1116};
int Freeverb::aDelayLengths[] = {225, 556, 441, 341};

Freeverb::Freeverb(void)
{
  Init(44100);
}

void Freeverb::Init(StkFloat sampleRate)
{
  this->sampleRate_ = sampleRate;
  // Initialize parameters
  effectMix_ = 0.75;                             // set initially to 3/4 wet 1/4 dry signal (different than original freeverb)
  roomSizeMem_ = (0.75 * scaleRoom) + offsetRoom; // feedback attenuation in LBFC
  dampMem_ = 0.25 * scaleDamp;                    // pole of lowpass filters in the LBFC
  width_ = 1.0;
  frozenMode_ = false;
  update();

  gain_ = fixedGain; // input gain before sending to filters
  g_ = 0.5;          // allpass coefficient, immutable in Freeverb

  // Scale delay line lengths according to the current sampling rate
  double fsScale = sampleRate_ / 44100.0;
  if (fsScale != 1.0)
  {
    // scale comb filter delay lines
    for (int i = 0; i < nCombs; i++)
    {
      cDelayLengths[i] = (int)floor(fsScale * cDelayLengths[i]);
    }

    // Scale allpass filter delay lines
    for (int i = 0; i < nAllpasses; i++)
    {
      aDelayLengths[i] = (int)floor(fsScale * aDelayLengths[i]);
    }
  }

  // Initialize delay lines for the LBFC filters
  for (int i = 0; i < nCombs; i++)
  {
    combDelayL_[i].setMaximumDelay(cDelayLengths[i]);
    combDelayL_[i].setDelay(cDelayLengths[i]);
    combDelayR_[i].setMaximumDelay(cDelayLengths[i] + stereoSpread);
    combDelayR_[i].setDelay(cDelayLengths[i] + stereoSpread);
  }

  // initialize delay lines for the allpass filters
  for (int i = 0; i < nAllpasses; i++)
  {
    allPassDelayL_[i].setMaximumDelay(aDelayLengths[i]);
    allPassDelayL_[i].setDelay(aDelayLengths[i]);
    allPassDelayR_[i].setMaximumDelay(aDelayLengths[i] + stereoSpread);
    allPassDelayR_[i].setDelay(aDelayLengths[i] + stereoSpread);
  }
}

Freeverb::~Freeverb()
{
}

void Freeverb::setEffectMix(StkFloat mix)
{
  effectMix_ = (mix);
  update();
}

void Freeverb::setRoomSize(StkFloat roomSize)
{
  roomSizeMem_ = (roomSize * scaleRoom) + offsetRoom;
  update();
}

StkFloat Freeverb::getRoomSize()
{
  return (roomSizeMem_ - offsetRoom) / scaleRoom;
}

void Freeverb::setDamping(StkFloat damping)
{
  dampMem_ = damping * scaleDamp;
  update();
}

StkFloat Freeverb::getDamping()
{
  return dampMem_ / scaleDamp;
}

void Freeverb::setWidth(StkFloat width)
{
  width_ = width;
  update();
}

StkFloat Freeverb::getWidth()
{
  return width_;
}

void Freeverb::setMode(bool isFrozen)
{
  frozenMode_ = isFrozen;
  update();
}

StkFloat Freeverb::getMode()
{
  return frozenMode_;
}

void Freeverb::update()
{
  StkFloat wet = scaleWet * effectMix_;
  dry_ = scaleDry * (1.0 - effectMix_);

  // Use the L1 norm so the output gain will sum to one while still
  // preserving the ratio of scalings in original Freeverb
  wet /= (wet + dry_);
  dry_ /= (wet + dry_);

  wet1_ = wet * (width_ / 2.0 + 0.5);
  wet2_ = wet * (1.0 - width_) / 2.0;

  if (frozenMode_)
  {
    // put into freeze mode
    roomSize_ = 1.0;
    damp_ = 0.0;
    gain_ = 0.0;
  }
  else
  {
    roomSize_ = roomSizeMem_;
    damp_ = dampMem_;
    gain_ = fixedGain;
  }

  for (int i = 0; i < nCombs; i++)
  {
    // set low pass filter for delay output
    combLPL_[i].setCoefficients(1.0 - damp_, -damp_);
    combLPR_[i].setCoefficients(1.0 - damp_, -damp_);
  }
}

void Freeverb::clear()
{
  // Clear LBFC delay lines
  for (int i = 0; i < nCombs; i++)
  {
    combDelayL_[i].clear();
    combDelayR_[i].clear();
  }

  // Clear allpass delay lines
  for (int i = 0; i < nAllpasses; i++)
  {
    allPassDelayL_[i].clear();
    allPassDelayR_[i].clear();
  }
}


