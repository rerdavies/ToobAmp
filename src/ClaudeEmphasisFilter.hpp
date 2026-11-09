/*
 *   Copyright (c) 2026 
 *   All rights reserved.
 */

#include <algorithm>
#include <cmath>


 class ClaudeEmphasisFilter {

public:
    ClaudeEmphasisFilter()
    {
        SetSampleRate(48000.0);
    }

    void SetSampleRate(double sampleRate) {
        constexpr double kOverallGainDb = -39.37303155466245;
        constexpr double kHighPassFcHz = 3.1;
        constexpr double kHighPassQ1 = 1.4678416324456842;
        constexpr double kHighPassQ2 = 0.05500192201908015;
        constexpr double kHighShelfFcHz = 3357.8585291373874;
        constexpr double kHighShelfQ = 0.4316679497711965;
        constexpr double kHighShelfGainDb = 17.537860013354326;

        sampleRate_ = std::max(1000.0, sampleRate);
        overallGain_ = std::pow(10.0, kOverallGainDb / 20.0);

        stage1_.SetHighPass(sampleRate_, kHighPassFcHz, kHighPassQ1);
        stage2_.SetHighPass(sampleRate_, kHighPassFcHz, kHighPassQ2);
        stage3_.SetHighShelf(sampleRate_, kHighShelfFcHz, kHighShelfQ, kHighShelfGainDb);

        stage1_.Reset();
        stage2_.Reset();
        stage3_.Reset();
    }

    double Tick(double value) { 
        double y = stage1_.Tick(value);
        y = stage2_.Tick(y);
        y = stage3_.Tick(y);
        return y * overallGain_;
    }

private:
    class Biquad {
    public:
        void SetHighPass(double sampleRate, double cutoffHz, double q)
        {
            const double nyquist = 0.5 * sampleRate;
            cutoffHz = std::clamp(cutoffHz, 1.0, nyquist * 0.45);
            q = std::max(q, 0.05);

            const double w0 = 2.0 * M_PI * cutoffHz / sampleRate;
            const double cs = std::cos(w0);
            const double sn = std::sin(w0);
            const double alpha = sn / (2.0 * q);

            const double b0 = 0.5 * (1.0 + cs);
            const double b1 = -(1.0 + cs);
            const double b2 = 0.5 * (1.0 + cs);
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * cs;
            const double a2 = 1.0 - alpha;

            SetNormalized(b0, b1, b2, a0, a1, a2);
        }

        void SetHighShelf(double sampleRate, double cutoffHz, double q, double gainDb)
        {
            const double nyquist = 0.5 * sampleRate;
            cutoffHz = std::clamp(cutoffHz, 1.0, nyquist * 0.45);
            q = std::max(q, 0.05);

            const double A = std::pow(10.0, gainDb / 40.0);
            const double w0 = 2.0 * M_PI * cutoffHz / sampleRate;
            const double cs = std::cos(w0);
            const double sn = std::sin(w0);
            const double alpha = sn / (2.0 * q);
            const double sqrtA = std::sqrt(A);

            const double b0 = A * ((A + 1.0) + (A - 1.0) * cs + 2.0 * sqrtA * alpha);
            const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
            const double b2 = A * ((A + 1.0) + (A - 1.0) * cs - 2.0 * sqrtA * alpha);
            const double a0 = (A + 1.0) - (A - 1.0) * cs + 2.0 * sqrtA * alpha;
            const double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
            const double a2 = (A + 1.0) - (A - 1.0) * cs - 2.0 * sqrtA * alpha;

            SetNormalized(b0, b1, b2, a0, a1, a2);
        }

        double Tick(double x)
        {
            const double y = b0_ * x + s1_;
            s1_ = b1_ * x - a1_ * y + s2_;
            s2_ = b2_ * x - a2_ * y;
            return y;
        }

        void Reset()
        {
            s1_ = 0.0;
            s2_ = 0.0;
        }

    private:
        void SetNormalized(double b0, double b1, double b2, double a0, double a1, double a2)
        {
            b0_ = b0 / a0;
            b1_ = b1 / a0;
            b2_ = b2 / a0;
            a1_ = a1 / a0;
            a2_ = a2 / a0;
        }

        double b0_ = 1.0;
        double b1_ = 0.0;
        double b2_ = 0.0;
        double a1_ = 0.0;
        double a2_ = 0.0;
        double s1_ = 0.0;
        double s2_ = 0.0;
    };

    double sampleRate_ = 48000.0;
    double overallGain_ = 1.0;
    Biquad stage1_;
    Biquad stage2_;
    Biquad stage3_;
 };