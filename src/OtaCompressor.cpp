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
#include <stdexcept>
#include "ss.hpp"

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
    }
    else
    {
        constexpr const float INPUT_VOLTAGE = 0.7;
        constexpr const float OUTPUT_VOLTAGE = (2.4 - 0.62) / 2;

        this->trim = Db2Af(db) * (OUTPUT_VOLTAGE / INPUT_VOLTAGE);
    }
}

void CompressorEnvelope::UpdateControls(double attackTime, double releaseTime)
{
    this->attackTime = std::max(attackTime, 1E-4);
    this->releaseTime = std::max(releaseTime, 1E-3);

    attackStep = DEFAULT_VOLTAGE / (this->attackTime * sampleRate * ATTACK_REFERENCE_EXCESS);
    releaseCoefficient = 1.0 - std::exp(-1.0 / (this->releaseTime * sampleRate));
}

double CompressorEnvelope::Update(double excess)
{
    // C5 recharges through R3 toward the supply voltage; Q4/Q5 drain it in proportion to the
    // amount by which the signal exceeds their turn-on voltage.
    if (excess < 0) throw std::runtime_error(SS("Invalid excess: " <<excess));
    double v = vEnvelope + (DEFAULT_VOLTAGE - vEnvelope) * releaseCoefficient - attackStep * excess;
    vEnvelope = std::clamp(v, 0.0, DEFAULT_VOLTAGE);
    return vEnvelope;
}

double CompressorEnvelope::Tick(double value)
{
    return Update(Excess(value));
}

double CompressorEnvelope::Tick(const StereoResult &input)
{
    return Update(0.73 * (Excess(input.left) + Excess(input.right)));
}

void CompressorEnvelope::Initialize(double sampleRate)
{
    this->sampleRate = sampleRate;
    UpdateControls(attackTime, releaseTime);
    Reset();
}

void CompressorEnvelope::Reset()
{
    vEnvelope = DEFAULT_VOLTAGE;
}

void OtaStage::SetSensitivity(double sensitivity)
{
    this->sensitivity = std::clamp(sensitivity, 0.0, 1.0);
    double rangeDb = MIN_RANGE_DB + (MAX_RANGE_DB - MIN_RANGE_DB) * this->sensitivity;
    this->gainRange = std::pow(10.0, rangeDb / 20.0) - 1.0;
}

void OtaEmphasisStage::Initialize(double sampleRate)
{
    // SPICE responses were measured with a -20dB input and are expressed as absolute output level.
    constexpr double INPUT_DB = -20;
    constexpr double PASSBAND_DB = -39.373;

    lowCut.SetHighPass(sampleRate, 30.0, 0.6);
    shelf.SetHighShelf(sampleRate, SHELF_FREQUENCY, SHELF_Q, SHELF_DB);
    gain = std::pow(10.0, (PASSBAND_DB - INPUT_DB) / 20.0);
    Reset();
}

void OtaEmphasisStage::Reset()
{
    lowCut.Reset();
    shelf.Reset();
}

void OtaOutputStage::Initialize(double sampleRate)
{
    constexpr double ROLLOFF_FREQUENCY = 8500;
    constexpr double DC_BLOCK_FREQUENCY = 10;
    constexpr double BUTTERWORTH_Q = 0.7071;

    deEmphasis.SetHighShelf(sampleRate, OtaEmphasisStage::SHELF_FREQUENCY, OtaEmphasisStage::SHELF_Q, -OtaEmphasisStage::SHELF_DB);
    // 4th-order Butterworth.
    lowPass1.SetLowPass(sampleRate, ROLLOFF_FREQUENCY, 0.5412);
    lowPass2.SetLowPass(sampleRate, ROLLOFF_FREQUENCY, 1.3066);
    dcBlock.SetHighPass(sampleRate, DC_BLOCK_FREQUENCY, BUTTERWORTH_Q);
    Reset();
}

void OtaOutputStage::Reset()
{
    deEmphasis.Reset();
    lowPass1.Reset();
    lowPass2.Reset();
    dcBlock.Reset();
}

void OtaCompressor::Initialize(double baseSampleRate)
{
    double sampleRate = baseSampleRate*SUPERSAMPLING_RATE;
    inputStage.Initialize(sampleRate);
    emphasisStage.Initialize(sampleRate);
    otaStage.Initialize(sampleRate);
    outputStage.Initialize(sampleRate);
    compressorEnvelope.Initialize(sampleRate);
    compressorEnvelope.UpdateControls(attackTime, releaseTime);
    Reset();
}

void OtaCompressor::Reset()
{
    inputStage.Reset();
    emphasisStage.Reset();
    otaStage.Reset();
    outputStage.Reset();
    compressorEnvelope.Reset();
    envelopeVoltage = CompressorEnvelope::DEFAULT_VOLTAGE;
    upsampler.Reset();
    downsampler.Reset();
}

inline float OtaCompressor::TickSupersampled(float value)
{
    double v1 = inputStage.Tick((double)value);
    double v2 = emphasisStage.Tick(v1);
    double v3 = otaStage.Tick(v2, envelopeVoltage);
    double v4 = outputStage.Tick(v3);
    // The gain applied to the next sample is determined by the output of this one.
    this->envelopeVoltage = compressorEnvelope.Tick(v4);
    return (float)(v4)*this->outputAdjustment;
}

inline StereoResult OtaCompressor::TickSupersampled(float left, float right)
{
    auto v1 = inputStage.Tick(StereoResult{(double)left, (double)right});
    auto v2 = emphasisStage.Tick(v1);
    auto v3 = otaStage.Tick(v2, envelopeVoltage);
    auto v4 = outputStage.Tick(v3);
    this->envelopeVoltage = compressorEnvelope.Tick(v4);
    v4.left *= outputAdjustment;
    v4.right *= outputAdjustment;
    // {
    //     static size_t count = 0;
    //     static size_t limit = 10000;
    //     if (count++ >= limit)
    //     {
    //         count = 0;
    //         std::cout << "VEnv: " << compressorEnvelope.Voltage() << std::endl;
    //     }
    // }
    return v4;
}

float OtaCompressor::Tick(float value)
{
    upsampler.TickInput(value);
    for (size_t i = 0; i < SUPERSAMPLING_RATE; ++i)
    {
        float supersampledResult = TickSupersampled(upsampler.TickOutput());
        downsampler.TickInput(supersampledResult);
    }
    return downsampler.TickOutput();
}

StereoResult OtaCompressor::Tick(float valueL, float valueR)
{
    upsampler.TickInput(valueL);
    upsamplerR.TickInput(valueR);

    for (size_t i = 0; i < SUPERSAMPLING_RATE; ++i)
    {
        auto supersampledResult = TickSupersampled(
            upsampler.TickOutput(),
            upsamplerR.TickOutput()
        );
        downsampler.TickInput(supersampledResult.left);
        downsamplerR.TickInput(supersampledResult.right);
    }
    return StereoResult{ downsampler.TickOutput(),downsamplerR.TickOutput()};
}

