/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#include "std.h"
#include "NoiseGate.h"
#include "LsNumerics/LsMath.hpp"



using namespace toob;
using namespace LsNumerics;

static const double ATTACK_SECONDS = 0.001;
static const double RELEASE_SECONDS = 0.3;
static const double HOLD_SECONDS = 0.2;


int32_t NoiseGate::SecondsToSamples(double seconds)
{
    return (int32_t)sampleRate*seconds;
}

void NoiseGate::SetGateThreshold(float decibels)
{
     this->afAttackThreshold = LsNumerics::Db2Af(decibels);
    this->afReleaseThreshold = this->afAttackThreshold*0.25f;
}
double NoiseGate::SecondsToRate(double seconds)
{
    return 1/(seconds*sampleRate);
}

void NoiseGate::SetSampleRate(double sampleRate)
{
    this->sampleRate = sampleRate;
    this->attackRate = SecondsToRate(ATTACK_SECONDS);
    this->releaseRate = SecondsToRate(RELEASE_SECONDS);
    this->holdSampleDelay = SecondsToSamples(HOLD_SECONDS);
}