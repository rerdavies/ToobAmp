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

namespace ota_compressor
{
    using namespace toob;
    
    struct StereoResult
    {
        double left;
        double right;
    };

    class CompressorEnvelope
    {
    public:
        void Initialize(double sampleRate);
        void UpdateControls(double attackTime, double sensitivity);
        void Reset();

        double Tick(double value);
        double Tick(const StereoResult&stereoResult);

    private:
        static constexpr double DEFAULT_VOLTAGE = 9.0f;
        static constexpr double DIODE_DROP_VOLTAGE = 0.4f;
        double attackRate = 1.0;
        double releaseRate = 1.0;
        double vEnvelope = DEFAULT_VOLTAGE;
    };


    class OtaStage {
    public:
        void Initialize(double sampleRate) { }
        void Reset() { }
        double Tick(double input, double gmod) { return input; }
        StereoResult Tick(const StereoResult& input, double gmod) { return input; }

    };

    class OtaInputStage {
    public:
        void Initialize(double sampleRate);
        void Reset();
        void SetTrimDb(float db);
        double Tick(double input) 
        {
            return lowCut.Tick(input)*trim;
        }
        StereoResult Tick(const StereoResult&input) 
        {
            return StereoResult{ 
                lowCut.Tick(input.left)*trim, 
                lowCut.TickR(input.right)*trim};
        }

        double Trim() const { return trim; }
    private:
        HighPassFilter lowCut;
        double trim;

    };

    template <float CLIP_VOLTAGE = 0.0f>
    class OtaDiodeClipStage {
    public:
        void Initialize(double sampleRate) { }
        void Reset() { }
        double Tick(double input) { 
            static constexpr float THERMAL_VOLTAGE = 0.026f;
            // Diode waveshape: V_out = V_t * ln(1 + exp(V_in / V_t))
            float clipped = THERMAL_VOLTAGE * std::log(1.0f + std::exp(std::max((float)input, CLIP_VOLTAGE) / THERMAL_VOLTAGE));
            return clipped; 
        }
        StereoResult Tick(const StereoResult& input) { 
            static constexpr float THERMAL_VOLTAGE = 0.026f;
            float leftClipped = THERMAL_VOLTAGE * std::log(1.0f + std::exp(std::max((float)input.left, CLIP_VOLTAGE) / THERMAL_VOLTAGE));
            float rightClipped = THERMAL_VOLTAGE * std::log(1.0f + std::exp(std::max((float)input.right, CLIP_VOLTAGE) / THERMAL_VOLTAGE));
            return StereoResult{leftClipped, rightClipped};
        }
    };
    class OtaEmphasisStage {
    public:
        void Initialize(double sampleRate);
        void Reset();

        inline float Tick(float value) {
            return gain*lowCut.Tick(shelf.Tick(value));
        }
        inline StereoResult Tick(const StereoResult&input) {
            double left = gain*lowCut.Tick(shelf.Tick(input.left));
            double right = gain*lowCut.TickR(shelf.TickR(input.right));
            return StereoResult{left,right};
        }
    private:
        double gain = 0.0;
        HighPassFilter lowCut;
        ShelvingFilter shelf;
    };
    class OtaCompressor
    {
    public:
        void Initialize(double sampleRate);

        void Reset();

        void SetInputTrim(float value) { inputStage.SetTrimDb(value); }

        float Tick(float value);
        StereoResult Tick(float left, float right);
    private:
        OtaInputStage inputStage; 
        OtaEmphasisStage emphasisStage;
        OtaStage otaStage;

        double otaGain = 1.0;

        CompressorEnvelope compressorEnvelope;
    };

}