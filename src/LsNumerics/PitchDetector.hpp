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

/********************************************************
 * class PitchDetector - pitch detector optimized for use as in a guitar tuner.
 *
 * PitchDetector currently only works with a sample rate of 22050, or 24000hz.
 * (There are manually fittted adjustments that improve pitch accuracy which
 * currently can't be accounted for in closed form).
 *
 * Downssampling is neccessary both for efficiency and stability. Running
 * pitch detection at higher sample rates does not increase accuracy,
 * and introduces instability problems that are not present at lower sample
 * rates.
 *
 * Ideally you should decimate your audio signal after running it through a
 * suitable lowpass anti-aliasing filter. Setting the anti-aliasing filter
 * fC to 1200hz would not be wrong, as the pitch detector doesn't work
 * well above that frequency.
 *
 * Measured accuracy: stable tuning with ~0.1 cents accuracy in normal guitar
 * tuning range. Less than 1 cent below high-e 12th fret, with osciallations
 * due to phase errors increasing above the 12th fret. You could filter to
 * reduce those oscillations if you REALLY need pitch detection in that range.
 *
 * NOT suitable for use on a realtime thread!

*/
#pragma once

#include <cstdint>
#include "Fft.hpp"
#include <vector>
#include "LsMath.hpp"

// auto-correlation was tested and found less accurate.
// keep the code for now.
#define LS_ENABLE_AUTO_CORRELATION_CODE 0

namespace LsNumerics
{

    /**
     * @brief Pitch Detector
     * 
     * PitchDetector is tuned for use as a guitar tuner. It detects the MIDI pitch of monophonic audio input data.
     * 
     * For best results, run PitchDetector at 22050 or 24000 Hz. Higher sample rates are not more accurate, 
     * are less noise-tolerant, and are O(n log(n)) more expensive to execute.
     * 
     * For best results, call PitchDector(sampleRate), or Initialize(sampleRate), and call PitchDetector::getFftSize()
     * to determine how much data has to be in the audio buffer supplied to detectPitch(). These overrides 
     * select an optimal size for the buffers. Currently, buffers are 4096 samples at 22050 or 24000Hz, but that may change in 
     * future releases, and increase linearly to the next power of 2 with higher sample rates.
     * 
     * PitchDetector typically requires about a fifth of a second of audio data. If you need updates at 
     * a faster rate, you may need to do overlapped calls to detectPitch. i.e. new audio data with enough 
     * old audio data to pad the buffer out to the correct size.
     * 
     * PitchDetector currently selects a buffer size of 4096 samples when running at 22050 or 24000Hz. This 
     * may change in future revisions of PitchDetector.
     * 
     * PitchDetector currently uses ceptstrum pitch detection to find the fundamental frequency of the input signal.
     * This provides an approximate pitch, which is then refined using Grandke interpolation.
     * 
     * Tested accuracy:  +/- 0.001 cents with no signal noise. Less than 0.1 cent with less than -35db SNR. Less than 1 cent 
     * with very occasional errors with less than -30db SNR. Range: 80hz to 993hz (range of a guitar from low E to 19th fret on 
     * the high E string). 
     * 
     */
    class PitchDetector
    {
        Fft fftPlan;

    private:
        int cepstrumFftSize;

        int minimumCepstrumBin;
        int maximumCepstrumBin;
        double frequencyAdustmentFactor = 0;

        double calculateFrequencyAdjustmentFactor();

        float referencePitch = 440.0f;

        bool debug;

        using complex = std::complex<double>;

        int sampleRate;
        std::vector<complex> stagingBuffer;

    public:
        using WindowT = std::vector<double>;

    private:
        WindowT window;
        std::vector<complex> conversionBuffer;
        std::vector<complex> scratchBuffer;
        std::vector<complex> fftBuffer;
        std::vector<complex> cepstrumBuffer;
        std::vector<double> cepstrum;

        std::vector<complex> lastFftBuffer;

#if LS_ENABLE_AUTO_CORRELATION_CODE
        Fft crossCorrelationFft;
        int crossCorrelationSize;
        int crossCorrelationSamples;
        std::vector<double> autoCorrelation;
        std::vector<double> speciallyNormalizedAutoCorrelation;
#endif
    public:
        WindowT &Window() { return window; }

        double getGrandkeEstimate(double minFrequency,double maxFrequency);
        double getGrandkeEstimate(double frequency);


    private:
        void allocateBuffers();
        size_t findCepstrumBin(std::vector<double> &cepstrum);
        double refineWithCrossCorrelation(std::vector<double> &crossCorrelation, double cepstrumFrequency);
    public:
        /**
         * @brief Construct a new Pitch Detector object
         * 
         * Initialize must be called before use.
         */
        PitchDetector();

        /**
         * @brief Initialize the pitch detector.
         * 
         * PitchDetector will choose the optimum FFT size for the selected sample rate.
         * 
         * For best results, choose a sample rate of either 22050 or 24000, and downsample 
         * (decimate) audio data when detecting pitch. 
         * 
         * Higher sample rates are O(n log n) more expensive, which is non-trivial, are more 
         * succeptible to signal noise, and are not more accurate.
         * 
         * @param sampleRate Audio sample rate.
         */
        void Initialize(int sampleRate);

        /**
         * @brief Initialize the pitch detector with an explicit FFT size.
         * 
         * Not recommended. Call Initialize(sampleRate) instead.
         * 
         * Larger FFT sizes do not result in increased accuracy.
         * 
         * @param sampleRate 
         * @param fftSize 
         */
        void Initialize(int sampleRate, int fftSize);

    public:
        
        /**
         * @brief Construct a new Pitch Detector object
         * 
         * The optimal fft size is chosen based on the sample rate. (Higher sample rates require larger FFTs).
         * 
         * For best results, sampleRate should be set to either 22050, or 24000, and sample data should be downsampled
         * to the appropriate rate. 
         * 
         * Higher sample rates are more sensitive to signal noise, are O(n log(n)) more expensive, and are *not* more accurate. 
         * 
         * @param sampleRate sample rate.
         */

        PitchDetector(int sampleRate);

        /**
         * @brief Construct a new Pitch Detector object with an explicit FFT size.
         * 
         * Not recommended. Use PitchDetector(sampleRate) instead, to allow PitchDetector
         * to choose an optimum FFT size.
         * 
         * @param sampleRate audio sample rate.
         * @param fftSize size of the FFT.
         */
        PitchDetector(int sampleRate, int fftSize);

        
        /**
         * @brief Get the number of samples in the FFT.
         * 
         * @return size_t Size of the internal FFT.
         */
        size_t getFftSize() const { return this->cepstrumFftSize; }

        /**
         * @brief Detect the pitch of the supplied audio data.
         * 
         * @param signal A buffer countaining audio data. Call getFftSize() to determine how
         *    many samples must be supplied.
         * @return double The MIDI pitch of the signal, or zero if no signal detected.
         */
        double detectPitch(short *signal)
        {
            for (int i = 0; i < cepstrumFftSize; ++i)
            {

                scratchBuffer[i] = window[i] * signal[i] * (1.0 / 32768); // non-linearity reduces cepstrum octave errors ;
            }

            return detectPitch();
        }

        double instantaneousPitch(int bin);

        /**
         * @brief Detect the pitch of the supplied audio data.
         * 
         * The ITERATOR must point to a collection of auido samples of either type float, or type double. Call getFftSize() to determine 
         * how many samples must be supplied.
         * 
         * @tparam ITERATOR A collection iterator.
         * @param begin The start of data.
         * @return double MIDI pitch of the signal, or zero if no signal detected.
         */

        template <typename ITERATOR>
        double detectPitch(const ITERATOR &begin)
        {
            ITERATOR iter = begin;
            for (int i = 0; i < cepstrumFftSize; ++i)
            {
                scratchBuffer[i] = window[i]*(*iter++);
            }
            return detectPitch();
        }

        /**
         * @brief Detect the pitch of the supplied audio data.
         * 
         * Call getFftSize() to determine how many samples must be supplied.
         * 
         * @param signal A buffer of audio data.
         * @return double MIDI pitch of the signal, or zero if no signal detected.
         */
        double detectPitch(float *signal)
        {
            for (int i = 0; i < cepstrumFftSize; ++i)
            {
                scratchBuffer[i] = window[i]* signal[i];
            }
            return detectPitch();
        }
        double detectPitch(float *signal, size_t sampleStride)
        {
            for (int i = 0; i < cepstrumFftSize; ++i)
            {
                scratchBuffer[i] = window[i]* signal[i];
            }
            return detectPitch(sampleStride);
        }

    private:
        double ifPhase(size_t bin);
        double detectPitch();
        double detectPitch(size_t sampleStride);

    public:
        /**
         * @brief Get the freqency of concert A.
         * 
         * Defaults to A440.
         * 
         * @return float Frequency of concert A. 
         */
        float getReferencePitch()
        {
            return referencePitch;
        }

        /**
         * @brief Set the frequency of concert A.
         * 
         * Defaults to A440.
         * 
         * @param referencePitch The frequency of concert A in Hz.
         */
        void setReferencePitch(float referencePitch)
        {
            this->referencePitch = referencePitch;
        }


    private:
        struct QuadResult
        {
            double x;
            double y;
        };

        double frequencyToBin(double frequency);
        double binToFrequency(double bin);


        double detectPeaks(std::vector<double> &x, double suggestedFrequency);

        bool findQuadraticMaximum(int binNumber, std::vector<double> &x, QuadResult &result);
        bool findQuadraticMaximumNoLog(int binNumber, std::vector<double> &x, QuadResult &result);
        bool findQuadraticMaximum(int binNumber, double p0, double p1, double p2, QuadResult &result);
    };
} // namespace.