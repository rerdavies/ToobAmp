/*
 *   Copyright (c) 2025 Robin E. R. Davies
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

/*
6-band octave graphic EQ based on

GRAPHIC EQUALIZER DESIGN USING HIGHER-ORDER RECURSIVE FILTERS,
Martin Holters, Udo Zölzer, Proc. of the 9th Int. Conference on
Digital Audio Effects (DAFx-06), Montreal, Canada, September 18-20, 2006.alignas
*/
#pragma once
#include <vector>
#include <memory>
#include <array>
#include <cmath>
#include <complex>
#include <cmath>
#include <cassert>

namespace toob::holters_graphic_eq
{

    inline bool f_compare(double v1, double v2, double delta)
    {
        return std::abs(v1 - v2) <= delta;
    }

    // Section for band-shelving filter
    // See [1] section 3.
    class GraphicEq;
    class ShelvingBandFilter;

    // biquad) coefficients
    class Biquad
    {
    public:
        double b0 = 1, b1 = 0, b2 = 0; // Numerator coefficients
        double a0 = 1, a1 = 0, a2 = 0; // Denominator coefficients
        double z1 = 0, z2 = 0;         // State variables for filter

        void reset()
        {
            z1 = 0;
            z2 = 0;
        }
        void setCoefficients(double b0, double b1, double b2, double a1, double a2)
        {
            this->b0 = b0;
            this->b1 = b1;
            this->b2 = b2;
            this->a0 = 1;
            this->a1 = a1;
            this->a2 = a2;
        }
        void setCoefficients(double b0, double b1, double b2, double a0, double a1, double a2)
        {
            double norm = 1 / a0;
            this->b0 = b0 * norm;
            this->b1 = b1 * norm;
            this->b2 = b2 * norm;
            this->a0 = 1;
            this->a1 = a1 * norm;
            this->a2 = a2 * norm;
        }
        double tick(double x)
        {
            // df II.
            double z0 = x - a1 * z1 - a2 * z2;
            double y = b0 * z0 + b1 * z1 + b2 * z2;
            z2 = z1;
            z1 = z0;
            return y;
        }
    };

    /**
     * @brief A(z) = z^1*( (cosOmegaM-z-1)/(1-cosOmegaM*z-1))
     *
     * See equation (16) in [1].
     *
     */

    class SectionAllpass
    {
    public:
        void setCosOmegaM(double cosOmegaM)
        {
            this->cosOmegaM = cosOmegaM;
        }

        void reset()
        {
            z = 0;
            z1 = 0;
        }
        double tick(double value)
        {
            // first z^-1
            // double x = z;
            // z = value;
            double x = value;

            // (cosOm-z^1)/ 1- cosOm*z-1

            // df II.
            // double z0 = x - a1 * z1 - a2 * z2;
            double z0 = x + cosOmegaM * z1;
            // double y = b0 * z0 + b1 * z1 + b2 * z2;
            double y = cosOmegaM * z0 - z1;
            z1 = z0;
            return y;
        }

    private:
        double z;
        double z1;

        double cosOmegaM;
    };
    class Section
    {
    public:
        Section() {}

        void init(
            ShelvingBandFilter *filter,
            size_t m,
            size_t M);

        void reset();
        void updateGainParams(double gain);
        double tick(double input);

    private:
        SectionAllpass allpass0;
        SectionAllpass allpass1;
        double zm1;
        double zm2;
        double apz1, apz2;

        double alpha_m;
        double c_m;
        double a0_m_inv;
        ShelvingBandFilter *filter;
    };

    class ShelvingBandFilter
    {
    public:
        size_t M = 6;
        size_t NUM_SECTIONS = M / 2;

        ShelvingBandFilter(double fs, double fLow, double fHi, size_t M = 6)
            : fs_(fs), gain_(-1), M(M)
        {
            NUM_SECTIONS = M / 2;
            init(fLow, fHi);
            setGain(1);
        }
        void init(double fLow, double fHi)
        {
            Omega_L = 2 * M_PI * fLow / fs_;
            Omega_U = 2 * M_PI * fHi / fs_;
            Omega_B = Omega_U - Omega_L;
            Omega_C = std::sqrt(Omega_U * Omega_L);

            double t = std::sqrt(std::tan(Omega_U / 2) * std::tan(Omega_L / 2));
            Omega_M = atan(t) * 2; // (19)

            assert(f_compare(pow(tan(Omega_M / 2), 2), tan(Omega_U / 2) + tan(Omega_L / 2), 1E5));

            tan_Omega_B_by_2 = tan(Omega_B / 2);

            sections.resize(M / 2);
            for (size_t i = 0; i < sections.size(); ++i)
            {
                sections[i].init(this, i + 1, M);
            }
        }

        void setGain(double gain)
        {
            if (gain != gain_)
            {
                gain_ = gain;
                this->K = pow(gain, -1.0 / (2.0 * M)) * tan_Omega_B_by_2; // (14)
                this->V = pow(gain, 1.0 / M) - 1;                         // after (11)

                for (auto &section : sections)
                {
                    section.updateGainParams(gain);
                }
            }
        }

        void reset()
        {
            for (auto &section : sections)
            {
                section.reset();
            }
        }

        double process(double x)
        {
            double y = x;

            for (auto &section : sections)
            {
                y = section.tick(y);
            }
            return y;
        }
        double getFrequencyResponse(double sampleRate, double freq)
        {
            double omega = 2 * M_PI * freq / sampleRate;
            // std::complex<double> ejOmega = std::exp(std::complex<double>(0.0,omega));

            double cos_Omega_M = std::cos(Omega_M);
            double cos_Omega = std::cos(omega);

            double t1 = pow(cos_Omega_M - cos_Omega, 2 * M);
            double t2 = pow(K * sin(omega), 2 * M);

            std::complex response =
                (t1 + t2 * gain_ * gain_) / (t1 + t2);
            return std::sqrt(std::abs(response));
        }
        double fs_; // Sampling rate
        double Omega_L, Omega_U, Omega_M, Omega_B, Omega_C;
        double gain_; // Gain in dB
        double tan_Omega_B_by_2;
        double K;
        double V;
        std::vector<Section> sections;
    };

    class GraphicEq
    {
    private:
        size_t NUM_BANDS = 10;
        double BANK_F0 = 30;
        double R = 2;
        // static constexpr int FILTER_ORDER = 8;
        // static constexpr int NUM_SECTIONS = FILTER_ORDER / 2; // Number of second-order sections per band

    public:
        GraphicEq(
            double sample_rate,
            size_t num_bands = 10,
            double fc0 = 30,
            double r = 2.0) : fs_(sample_rate),
                              NUM_BANDS(num_bands),
                              BANK_F0(fc0),
                              R(r)
        {
            // Octave-spaced center frequencies starting at 100 Hz
            std::vector<double> frequencies;

            double freq = BANK_F0;
            for (size_t i = 0; i < NUM_BANDS; ++i)
            {
                frequencies.push_back(freq);
                freq *= R;
            }
            double sqrtR = std::sqrt(R);
            for (size_t i = 0; i < NUM_BANDS; ++i)
            {
                double fLow, fHi;

                double fC = frequencies[i];
                fLow = fC / sqrtR;
                fHi = fC * sqrtR;

                if (i == NUM_BANDS - 1 && NUM_BANDS != 1)
                {
                    fHi = 20000;
                }

                filters_.emplace_back(
                    std::make_unique<ShelvingBandFilter>(
                        fs_, fLow, fHi));
            }
        }
        float getSampleRate() const
        {
            return this->fs_;
        }
        // Set the gain for a specific band (0 to NUM_BANDS-1)
        void setGain(uint32_t band, double gain)
        {
            if (band >= 0 && band < NUM_BANDS)
            {
                filters_[band]->setGain(gain);
            }
        }

        void setLevel(double level)
        {
            this->level_ = level;
        }

        // Process a block of audio samples
        void process(const float *input, float *output, size_t n_samples)
        {
            for (size_t i = 0; i < n_samples; ++i)
            {
                double x = input[i];
                double y = x;
                for (auto &filter : filters_)
                {
                    y = filter->process(y);
                }
                output[i] = static_cast<float>(y);
            }
        }

        // Reset filter states (e.g., for initialization or after discontinuity)
        void reset()
        {
            for (auto &filter : filters_)
            {
                filter->reset();
            }
        }

        // Get the number of bands
        size_t getNumBands()
        {
            return NUM_BANDS;
        }

        std::vector<std::unique_ptr<ShelvingBandFilter>> &bandFilters()
        {
            return filters_;
        }
        double getFrequencyResponse(double freq)
        {
            double result = 1;
            for (auto &bandFilter : this->bandFilters())
            {
                result *= bandFilter->getFrequencyResponse(getSampleRate(), freq);
            }
            return result;
        }

    private:
        static double MidFrequency(double f0, double f1)
        {
            return std::sqrt(f0 * +f1);
        }

        double level_ = 1.0;
        double fs_; // Sampling rate
        std::vector<std::unique_ptr<ShelvingBandFilter>> filters_;
    };

    // //////////////////

    inline double Section::tick(double input)
    {

        // Translate figure 1 from [1] into code.
        // why is this so crazily difficult? :-/

        double K = filter->K;
        double V = filter->V;
        double zm1X2 = zm1 * 2;
        double t42 = zm2 + zm1X2;

        double t12 =
            zm2 - zm1X2 + K * (-2 * c_m * zm2 + zm1X2 * K);
        ;
        double t21 = (input * K - t12) * a0_m_inv;

        double t21Check = (input * K - (zm2 - 2 * zm1 + K * (-2 * c_m * zm2 + 2 * K * zm1))) * a0_m_inv;

        assert(f_compare(t21, t21Check, 1E-3));
        (void)t21Check;

        double t51 =
            (t21 + t42) * K;

        double tOut =
            ((t51 * V) + (t51 +
                          (zm2 - t21) * (-c_m)) *
                             2) *
                V +
            input;

        double tOutCheck =
            t51 * V * V + ((-c_m) * (zm2 - t21) + t51) * 2 * V + input;

        assert(f_compare(tOut, tOutCheck, 1E-3));
        (void)tOutCheck;
        zm2 = allpass1.tick(zm1);
        zm1 = allpass0.tick(t21);

        return tOut;
    }
    inline void Section::reset()
    {
        zm1 = 0;
        zm2 = 0;
        apz1 = 0;
        apz2 = 0;

        allpass0.reset();
        allpass1.reset();
    }

    inline void Section::init(
        ShelvingBandFilter *filter,
        size_t m, size_t M)
    {
        this->filter = filter;
        alpha_m = (0.5 - (2.0 * m - 1) / (2.0 * M)) * M_PI; // (9)
        c_m = std::cos(alpha_m);                            // after (11)
        double cos_Omega_M = std::cos(filter->Omega_M);
        allpass0.setCosOmegaM(cos_Omega_M);
        allpass1.setCosOmegaM(cos_Omega_M);

        reset();
        // (void)cos_Omega_M;
        // allpass0.setCoefficients(
        //     0, 1, 0,
        //     1, 0, 0);
        // allpass1.setCoefficients(
        //     0, 1, 0,
        //     1, 0, 0);
    }

    void Section::updateGainParams(double gain)
    {
        auto K = filter->K;
        a0_m_inv = 1.0 / (1.0 + 2.0 * K * c_m + K * K); // (17)
    }

} // namespace