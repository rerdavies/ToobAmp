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

#pragma once

#include "Filters/LowPassFilter.h"
#include "Filters/HighPassFilter.h"
#include "Filters/ShelvingFilter.h"
#include "LsNumerics/LsMath.hpp"
#include "LsNumerics/PowerOfTwoResampler.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

// Discrete-time model of the MXR Dyna Comp (CA3080 OTA compressor).
// See https://rerdavies.github.io/MxrDynaComp for the circuit analysis this is based on.
//
// Signal flow (all voltages are circuit volts):
//
//   input -> OtaInputStage -> OtaEmphasisStage -> OtaStage (gain + waveshaping) 
//         -> OtaOutputStage -> output
//                                   |
//                                   +--> CompressorEnvelope --> (gain control voltage) --> OtaStage
//
// The envelope generator is a leaky capacitor (C5). Whenever the output signal exceeds the
// base-emitter threshold of Q4/Q5, charge is drained from the capacitor (attack). The capacitor
// recharges through R3 (release). The capacitor voltage controls the OTA's bias current, and
// therefore its gain.
namespace ota_compressor
{
    using namespace toob;

    struct StereoResult
    {
        double left;
        double right;
    };

    // Direct form II transposed biquad, with independent state for a left and right channel.
    class Biquad
    {
    public:
        void SetHighPass(double sampleRate, double cutoffHz, double q)
        {
            double w0, cs, alpha;
            Prewarp(sampleRate, cutoffHz, q, &w0, &cs, &alpha);
            Set(0.5 * (1.0 + cs), -(1.0 + cs), 0.5 * (1.0 + cs),
                1.0 + alpha, -2.0 * cs, 1.0 - alpha);
        }
        void SetLowPass(double sampleRate, double cutoffHz, double q)
        {
            double w0, cs, alpha;
            Prewarp(sampleRate, cutoffHz, q, &w0, &cs, &alpha);
            Set(0.5 * (1.0 - cs), 1.0 - cs, 0.5 * (1.0 - cs),
                1.0 + alpha, -2.0 * cs, 1.0 - alpha);
        }
        // A negative gainDb gives the exact inverse of the same shelf with a positive gain.
        void SetHighShelf(double sampleRate, double cutoffHz, double q, double gainDb)
        {
            double w0, cs, alpha;
            Prewarp(sampleRate, cutoffHz, q, &w0, &cs, &alpha);
            const double A = std::pow(10.0, gainDb / 40.0);
            const double k = 2.0 * std::sqrt(A) * alpha;
            Set(A * ((A + 1.0) + (A - 1.0) * cs + k),
                -2.0 * A * ((A - 1.0) + (A + 1.0) * cs),
                A * ((A + 1.0) + (A - 1.0) * cs - k),
                (A + 1.0) - (A - 1.0) * cs + k,
                2.0 * ((A - 1.0) - (A + 1.0) * cs),
                (A + 1.0) - (A - 1.0) * cs - k);
        }

        void Reset()
        {
            s1 = s2 = s1R = s2R = 0;
        }

        double Tick(double x)
        {
            const double y = b0 * x + s1;
            s1 = LsNumerics::Undenormalize(b1 * x - a1 * y + s2);
            s2 = LsNumerics::Undenormalize(b2 * x - a2 * y);
            return y;
        }
        double TickR(double x)
        {
            const double y = b0 * x + s1R;
            s1R = LsNumerics::Undenormalize(b1 * x - a1 * y + s2R);
            s2R = LsNumerics::Undenormalize(b2 * x - a2 * y);
            return y;
        }

    private:
        static void Prewarp(double sampleRate, double cutoffHz, double q, double *w0, double *cs, double *alpha)
        {
            cutoffHz = std::clamp(cutoffHz, 1.0, sampleRate * 0.45);
            *w0 = 2.0 * std::numbers::pi * cutoffHz / sampleRate;
            *cs = std::cos(*w0);
            *alpha = std::sin(*w0) / (2.0 * std::max(q, 0.05));
        }
        void Set(double b0, double b1, double b2, double a0, double a1, double a2)
        {
            this->b0 = b0 / a0;
            this->b1 = b1 / a0;
            this->b2 = b2 / a0;
            this->a1 = a1 / a0;
            this->a2 = a2 / a0;
        }
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double s1 = 0, s2 = 0, s1R = 0, s2R = 0;
    };

    // The envelope generator: Q4/Q5 drain C5 when the signal exceeds their turn-on voltage;
    // C5 recharges toward the supply through R3.
    class CompressorEnvelope
    {
    public:
        static constexpr double DEFAULT_VOLTAGE = 9.0;
        static constexpr double DIODE_DROP_VOLTAGE = 0.25;
        static constexpr double DEFAULT_ATTACK_TIME = 0.002;
        static constexpr double DEFAULT_RELEASE_TIME = 1.74;

        void Initialize(double sampleRate);
        // attackTime: approximate time (seconds) for the envelope to attack when the signal exceeds
        // the detector threshold by ATTACK_REFERENCE_EXCESS volts.
        // releaseTime: time constant (seconds) of the capacitor recharge.
        void UpdateControls(double attackTime, double releaseTime);
        void Reset();

        // Returns the capacitor voltage (0 to DEFAULT_VOLTAGE).
        double Tick(double value);
        double Tick(const StereoResult &stereoResult);

        double Voltage() const { return vEnvelope; }

    private:
        static constexpr double ATTACK_REFERENCE_EXCESS = 0.5;

        static double Excess(double value)
        {
            double v = std::abs(value);
            return v > DIODE_DROP_VOLTAGE ? v - DIODE_DROP_VOLTAGE : 0.0;
        }
        double Update(double excess);

        double sampleRate = 48000.0;
        double attackTime = DEFAULT_ATTACK_TIME;
        double releaseTime = DEFAULT_RELEASE_TIME;
        double attackStep = 0.0;  // volts of capacitor discharge per volt of excess, per sample.
        double releaseCoefficient = 0.0;
        double vEnvelope = DEFAULT_VOLTAGE;
    };

    // The CA3080: a voltage-controlled gain, followed by the waveshaping of the output current.
    class OtaStage
    {
    public:
        static constexpr double POSITIVE_CLIP_VOLTAGE = 2.4;
        static constexpr double NEGATIVE_CLIP_VOLTAGE = 1.8;
        static constexpr double DIODE_KNEE_VOLTAGE = 0.12;

        void Initialize(double sampleRate) { SetSensitivity(sensitivity); }
        void Reset() {}

        // 0 to 1. Scales the range of OTA gain the envelope can control.
        void SetSensitivity(double sensitivity);
        double Sensitivity() const { return sensitivity; }

        // envelopeVoltage: the voltage on C5 (0 to CompressorEnvelope::DEFAULT_VOLTAGE).
        double Gain(double envelopeVoltage) const
        {
            return minGain * (1.0 + gainRange * envelopeVoltage 
                * (1.0 / CompressorEnvelope::DEFAULT_VOLTAGE));
        }

        // Output of the OTA (V) for a given gain-scaled differential input (V).
        // Positive excursions saturate like the tanh of a long-tailed pair (atan here); negative
        // excursions are cut off sharply by a diode-like exponential knee.
        static double Waveshape(double x)
        {
            if (x >= 0)
            {
                constexpr double K = POSITIVE_CLIP_VOLTAGE * 2.0 / std::numbers::pi;
                return K * std::atan(x / K);
            }
            // softplus: Vk * ln(1+exp(z/Vk)), offset to saturate at -NEGATIVE_CLIP_VOLTAGE.
            double z = (x + NEGATIVE_CLIP_VOLTAGE) / DIODE_KNEE_VOLTAGE;
            double softplus = (z > 0 ? z : 0.0) + std::log1p(std::exp(-std::abs(z)));
            return DIODE_KNEE_VOLTAGE * softplus - NEGATIVE_CLIP_VOLTAGE;
        }

        double Tick(double input, double envelopeVoltage)
        {
            return Waveshape(input * Gain(envelopeVoltage));
        }
        StereoResult Tick(const StereoResult &input, double envelopeVoltage)
        {
            double g = Gain(envelopeVoltage);
            return StereoResult{Waveshape(input.left * g), Waveshape(input.right * g)};
        }

    private:
        // Gain with the envelope fully attacked. Chosen so that a -12 dBV(pp) sine from a
        // single-coil pickup produces about 0.3V at the output when the envelope is attacked.
        static constexpr double OTA_MIN_GAIN = 16.0;
        static constexpr double MIN_RANGE_DB = 8.0;
        static constexpr double MAX_RANGE_DB = 32.0;

        double sensitivity = 1.0;
        double minGain = 0; // gain at envelopeCtl=9V.
    };

    class OtaInputStage
    {
    public:
        void Initialize(double sampleRate);
        void Reset();
        void SetTrimDb(float db);
        double Tick(double input)
        {
            return lowCut.Tick(input) * trim;
        }
        StereoResult Tick(const StereoResult &input)
        {
            return StereoResult{
                lowCut.Tick(input.left) * trim,
                lowCut.TickR(input.right) * trim};
        }

        double Trim() const { return trim; }

    private:
        HighPassFilter lowCut;
        double trim = 1.0;
    };

    // The passive network ahead of the OTA's differential input: low-cut, and a high-frequency
    // boost of about 17 dB (fitted to SPICE simulation of the circuit).
    class OtaEmphasisStage
    {
    public:
        void Initialize(double sampleRate);
        void Reset();

        static constexpr double SHELF_FREQUENCY = 3357.86;
        static constexpr double SHELF_Q = 0.43167;
        static constexpr double SHELF_DB = 17.5379;

        inline double Tick(double value)
        {
            return gain * shelf.Tick(lowCut.Tick(value));
        }
        inline StereoResult Tick(const StereoResult &input)
        {
            return StereoResult{
                gain * shelf.Tick(lowCut.Tick(input.left)),
                gain * shelf.TickR(lowCut.TickR(input.right))};
        }

    private:
        double gain = 0.0;
        Biquad lowCut;
        Biquad shelf;
    };

    // Filtering of the OTA output: the emphasis boost does not appear in the output voltage
    // (a high shelf cut), the output rolls off above 8.5kHz (a 4th-order low-pass), and a DC
    // blocker.
    class OtaOutputStage
    {
    public:
        void Initialize(double sampleRate);
        void Reset();

        inline double Tick(double value)
        {
            return dcBlock.Tick(lowPass2.Tick(lowPass1.Tick(deEmphasis.Tick(value))));
        }
        inline StereoResult Tick(const StereoResult &input)
        {
            return StereoResult{
                dcBlock.Tick(lowPass2.Tick(lowPass1.Tick(deEmphasis.Tick(input.left)))),
                dcBlock.TickR(lowPass2.TickR(lowPass1.TickR(deEmphasis.TickR(input.right))))};
        }

    private:
        Biquad deEmphasis;
        Biquad lowPass1;
        Biquad lowPass2;
        Biquad dcBlock;
    };

    class OtaCompressor
    {
    public:
        void Initialize(double sampleRate);

        void Reset();

        void SetInputTrim(float value) { inputStage.SetTrimDb(value); }
        // 0 to 1.
        void SetSensitivity(float value) { otaStage.SetSensitivity(value); }
        void SetAttackTime(double seconds)
        {
            attackTime = seconds;
            compressorEnvelope.UpdateControls(attackTime, releaseTime);
        }
        void SetReleaseTime(double seconds)
        {
            releaseTime = seconds;
            compressorEnvelope.UpdateControls(attackTime, releaseTime);
        }

        float Tick(float value);
        StereoResult Tick(float left, float right);

        float OutputAdjustment() const { return outputAdjustment;}
        void OutputAdjustment(float value) { outputAdjustment = value; }

        float EnvelopeVoltage() const { return envelopeVoltage; }
    private:
        float TickSupersampled(float value);
        StereoResult TickSupersampled(float left, float right);


        static constexpr int SUPERSAMPLING_RATE = 4;

        lsnumerics::PowerOfTwoUpsampler<SUPERSAMPLING_RATE> upsampler;
        lsnumerics::PowerOfTwoUpsampler<SUPERSAMPLING_RATE> upsamplerR;
        lsnumerics::PowerOfTwoDownsampler<SUPERSAMPLING_RATE> downsampler;
        lsnumerics::PowerOfTwoDownsampler<SUPERSAMPLING_RATE> downsamplerR;

        float outputAdjustment = Db2Af(-12.0f);
        double attackTime = CompressorEnvelope::DEFAULT_ATTACK_TIME;
        double releaseTime = CompressorEnvelope::DEFAULT_RELEASE_TIME;

        OtaInputStage inputStage;
        OtaEmphasisStage emphasisStage;
        OtaStage otaStage;
        OtaOutputStage outputStage;
        CompressorEnvelope compressorEnvelope;

        double envelopeVoltage = CompressorEnvelope::DEFAULT_VOLTAGE;
    };

}
