/*
 *   Copyright (c) 2026 Robin E. R. Davies
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


#include "OtaCompressor.hpp"
#include "LsNumerics/LsMath.hpp"

using namespace ota_compressor;


void OtaInputStage::Initialize(double sampleRate)
{
    this->lowCut.SetSampleRate(sampleRate);
    this->lowCut.SetCutoffFrequency(3.1);
    this->SetTrimDb(0);
    Reset();
}

void OtaInputStage::Reset() 
{
    this->lowCut.Reset();
}

void OtaInputStage::SetTrimDb(float db)
{
    if (db <= -120.0)
    {
        this->trim = 0;
    } else {
        constexpr const float INPUT_VOLTAGE = 0.7;
        constexpr const float OUTPUT_VOLTAGE = (2.4-0.62)/2;

        this->trim = Db2Af(db) * (OUTPUT_VOLTAGE/INPUT_VOLTAGE);
    }
}

        void Initialize(double sampleRate);
        void Reset();

void CompressorEnvelope::UpdateControls(double attackTime, double sensitivity)
{
  
}

double CompressorEnvelope::Tick(double value)
{
    float dv = 0;;
    if (value > DIODE_DROP_VOLTAGE) 
    {
        dv = value-DIODE_DROP_VOLTAGE;
    } else if (value < -DIODE_DROP_VOLTAGE) 
    {
        dv = -DIODE_DROP_VOLTAGE-value;
    }
    double result = (vEnvelope +attackRate*dv)*releaseRate;
    if (result > DEFAULT_VOLTAGE) {
        result = DEFAULT_VOLTAGE;
    }
    return result;
}
inline double CompressorEnvelope::Tick(const StereoResult&input)
{
    return Tick((input.left+input.right)*0.5);
}


void CompressorEnvelope::Initialize(double sampleRate) 
{
    Reset();
}

void CompressorEnvelope::Reset()
{
    vEnvelope = DEFAULT_VOLTAGE;
}

void OtaEmphasisStage::Initialize(double sampleRate)
{
    constexpr float INPUT_DB = -20;
    constexpr float MID_DB = -36;
    constexpr float SHELF_DB = -22;

    lowCut.SetSampleRate(sampleRate);
    lowCut.SetCutoffFrequency(30.0);
    shelf.SetSampleRate(sampleRate);
    shelf.SetHighShelf((SHELF_DB-MID_DB)/2,3000);
    gain = Db2Af(MID_DB-INPUT_DB);
    Reset();
}

void OtaEmphasisStage::Reset()
{
    lowCut.Reset();
    shelf.Reset();
}
void OtaCompressor::Initialize(double sampleRate)
{
    inputStage.Initialize(sampleRate);
    emphasisStage.Initialize(sampleRate);
    otaStage.Initialize(sampleRate);
    compressorEnvelope.Initialize(sampleRate);
}

void OtaCompressor::Reset() {
    inputStage.Reset();
    emphasisStage.Reset();
    otaStage.Reset();
    compressorEnvelope.Reset();
}


float OtaCompressor::Tick(float value)
{
    auto v1 = inputStage.Tick(value);
    auto v2 = emphasisStage.Tick(v1);
    auto v3 = otaStage.Tick(v2, this->otaGain);
    this->otaGain = compressorEnvelope.Tick(v3);
    return (float)v3;
}

inline StereoResult OtaCompressor::Tick(float left, float right)
{
    auto v1 = inputStage.Tick(StereoResult{(double)left, (double)right});
    auto v2 = emphasisStage.Tick(v1);
    auto v3 = otaStage.Tick(v2, this->otaGain);
    this->otaGain = compressorEnvelope.Tick(v3);
    return v3;


}

