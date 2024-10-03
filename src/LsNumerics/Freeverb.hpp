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
   https://github.com/thestk/stk
*/

#pragma once
#ifndef STK_FREEVERB_H
#define STK_FREEVERB_H

#include <vector>
#include <exception>
#include <sstream>

namespace LsNumerics
{
    using StkFloat = float;

    namespace stk
    {
        class Delay
        {
        public:
            //! The default constructor creates a delay-line with maximum length of 4095 samples and zero delay.
            /*!
              An StkError will be thrown if the delay parameter is less than
              zero, the maximum delay parameter is less than one, or the delay
              parameter is greater than the maxDelay value.
             */
            Delay(unsigned long delay = 0, unsigned long maxDelay = 0)
            {
                // Writing before reading allows delays from 0 to length-1.
                // If we want to allow a delay of maxDelay, we need a
                // delay-line of length = maxDelay+1.
                if (delay > maxDelay)
                {
                    throw std::invalid_argument(
                        "Delay::Delay: maxDelay must be > than delay argument!");
                }

                if ((maxDelay + 1) > inputs_.size())
                    inputs_.resize(maxDelay + 1);

                inPoint_ = 0;
                this->setDelay(delay);
            }

            //! Class destructor.
            ~Delay()
            {
            }
            void clear()
            {
                for (size_t i = 0; i < inputs_.size(); ++i)
                {
                    inputs_[i] = 0;
                }
            }
            StkFloat nextOut( void ) { return inputs_[outPoint_]; };
            //! Get the maximum delay-line length.
            unsigned long getMaximumDelay(void) { return inputs_.size() - 1; };

            //! Set the maximum delay-line length.
            /*!
              This method should generally only be used during initial setup
              of the delay line.  If it is used between calls to the tick()
              function, without a call to clear(), a signal discontinuity will
              likely occur.  If the current maximum length is greater than the
              new length, no memory allocation change is made.
            */
            void setMaximumDelay(unsigned long delay)
            {
                inputs_.resize(delay + 1);
            }

            //! Set the delay-line length.
            /*!
              The valid range for \e delay is from 0 to the maximum delay-line length.
            */
            void setDelay(unsigned long delay)
            {
                // read chases write
                if (inPoint_ >= delay)
                    outPoint_ = inPoint_ - delay;
                else
                    outPoint_ = inputs_.size() + inPoint_ - delay;
                delay_ = delay;
            }

            //! Return the current delay-line length.
            unsigned long getDelay(void) const { return delay_; };

            //! Return the value at \e tapDelay samples from the delay-line input.
            /*!
              The tap point is determined modulo the delay-line length and is
              relative to the last input value (i.e., a tapDelay of zero returns
              the last input value).
            */
            StkFloat tapOut(unsigned long tapDelay)
            {
                long tap = inPoint_ - tapDelay - 1;
                while (tap < 0) // Check for wraparound.
                    tap += inputs_.size();

                return inputs_[tap];
            }

            //! Set the \e value at \e tapDelay samples from the delay-line input.
            void tapIn(StkFloat value, unsigned long tapDelay)
            {
                long tap = inPoint_ - tapDelay - 1;
                while (tap < 0) // Check for wraparound.
                    tap += inputs_.size();

                inputs_[tap] = value;
            }

            //! Sum the provided \e value into the delay line at \e tapDelay samples from the input.
            /*!
              The new value is returned.  The tap point is determined modulo
              the delay-line length and is relative to the last input value
              (i.e., a tapDelay of zero sums into the last input value).
            */
            StkFloat addTo(StkFloat value, unsigned long tapDelay)
            {
                long tap = inPoint_ - tapDelay - 1;
                while (tap < 0) // Check for wraparound.
                    tap += inputs_.size();

                return inputs_[tap] += value;
            }

            //! Input one sample to the filter and return one output.
            StkFloat tick(StkFloat input);

        protected:
            std::vector<StkFloat> inputs_;
            unsigned long inPoint_;
            unsigned long outPoint_;
            unsigned long delay_;
        };

        inline StkFloat Delay ::tick(StkFloat input)
        {
            inputs_[inPoint_++] = input;

            // Check for end condition
            if (inPoint_ == inputs_.size())
                inPoint_ = 0;

            // Read out next value
            StkFloat t = inputs_[outPoint_++];

            if (outPoint_ == inputs_.size())
                outPoint_ = 0;

            return t;
        }

        class OnePole
        {
        public:
            //! The default constructor creates a low-pass filter (pole at z = 0.9).
            OnePole(StkFloat thePole = 0.9)
            {
                b_.resize(1);
                a_.resize(2);
                a_[0] = 1.0;

                this->setPole(thePole);
            }

            //! Class destructor.
            ~OnePole() {}

            //! Set the b[0] coefficient value.
            void setB0(StkFloat b0) { b_[0] = b0; };

            //! Set the a[1] coefficient value.
            void setA1(StkFloat a1) { a_[1] = a1; };

            //! Set all filter coefficients.
            void setCoefficients(StkFloat b0, StkFloat a1, bool clearState = false)
            {
                b_[0] = b0;
                a_[1] = a1;

                if ( clearState ) this->clear();                
            }

            void clear()
            {
                lastOutput_ = 0;
            }

            //! Set the pole position in the z-plane.
            /*!
              This method sets the pole position along the real-axis of the
              z-plane and normalizes the coefficients for a maximum gain of one.
              A positive pole value produces a low-pass filter, while a negative
              pole value produces a high-pass filter.  This method does not
              affect the filter \e gain value. The argument magnitude should be
              less than one to maintain filter stability.
            */
            void setPole(StkFloat thePole)
            {
                if (std::abs(thePole) >= 1.0)
                {
                    throw std::invalid_argument("OnePole::setPole: argument thePole should be less than 1.0!");
                }

                // Normalize coefficients for peak unity gain.
                if (thePole > 0.0)
                    b_[0] = (StkFloat)(1.0 - thePole);
                else
                    b_[0] = (StkFloat)(1.0 + thePole);

                a_[1] = -thePole;
            }

            //! Input one sample to the filter and return one output.
            StkFloat tick(StkFloat input);

        private:
            std::vector<StkFloat> a_;
            std::vector<StkFloat> b_;
            StkFloat lastOutput_ = 0;
        };

        inline StkFloat OnePole ::tick(StkFloat input)
        {
            StkFloat lastFrame_ = b_[0] * input - a_[1] * lastOutput_;
            lastOutput_ = lastFrame_;

            return lastFrame_;
        }

    }

    using namespace stk;

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

    class Freeverb
    {

    public:
        //! Freeverb Constructor
        /*!
          Initializes the effect with default parameters. Note that these defaults
          are slightly different than those in the original implementation of
          Freeverb [Effect Mix: 0.75; Room Size: 0.75; Damping: 0.25; Width: 1.0;
          Mode: freeze mode off].
        */
        Freeverb(StkFloat sampleRate = 44100);

        void Init(StkFloat ampleRate);

        //! Destructor
        ~Freeverb();

        //! Set the effect mix [0 = mostly dry, 1 = mostly wet].
        void setEffectMix(StkFloat mix);

        //! Set the room size (comb filter feedback gain) parameter [0,1].
        void setRoomSize(StkFloat value);

        //! Get the room size (comb filter feedback gain) parameter.
        StkFloat getRoomSize(void);

        //! Set the damping parameter [0=low damping, 1=higher damping].
        void setDamping(StkFloat value);

        //! Get the damping parameter.
        StkFloat getDamping(void);

        //! Set the width (left-right mixing) parameter [0,1].
        void setWidth(StkFloat value);

        //! Get the width (left-right mixing) parameter.
        StkFloat getWidth(void);

        //! Set the mode [frozen = 1, unfrozen = 0].
        void setMode(bool isFrozen);

        //! Get the current freeze mode [frozen = 1, unfrozen = 0].
        StkFloat getMode(void);

        //! Clears delay lines, etc.
        void clear(void);

        void tick(
            StkFloat inputL, StkFloat inputR,
            StkFloat *pOutL, StkFloat *pOutR);

    protected:
        //! Update interdependent parameters.
        void update(void);

        static inline StkFloat undenormalize(  StkFloat s ) {
            return s; // switched to turning off denorms in machine status word instead.
        }

        static const int nCombs = 8;
        static const int nAllpasses = 4;
        static const int stereoSpread = 23;
        static const StkFloat fixedGain;
        static const StkFloat scaleWet;
        static const StkFloat scaleDry;
        static const StkFloat scaleDamp;
        static const StkFloat scaleRoom;
        static const StkFloat offsetRoom;


        // Delay line lengths for 44100Hz sampling rate.
        static const int kcDelayLengths[nCombs];
        static const int kaDelayLengths[nAllpasses];

        int m_cDelayLengths[nCombs];
        int m_aDelayLengths[nAllpasses];
        StkFloat sampleRate_;
        StkFloat effectMix_;
        StkFloat g_; // allpass coefficient
        StkFloat gain_;
        StkFloat roomSizeMem_, roomSize_;
        StkFloat dampMem_, damp_;
        StkFloat wet1_, wet2_;
        StkFloat dry_;
        StkFloat width_;
        bool frozenMode_;

        // LBFC: Lowpass Feedback Comb Filters
        Delay combDelayL_[nCombs];
        Delay combDelayR_[nCombs];
        OnePole combLPL_[nCombs];
        OnePole combLPR_[nCombs];

        // AP: Allpass Filters
        Delay allPassDelayL_[nAllpasses];
        Delay allPassDelayR_[nAllpasses];
    };

    inline void Freeverb::tick(
        StkFloat inputL, StkFloat inputR,
        StkFloat *pOutL, StkFloat *pOutR)
    {
#if defined(_STK_DEBUG_)
        if (channel > 1)
        {
            oStream_ << "Freeverb::tick(): channel argument must be less than 2!";
            handleError(StkError::FUNCTION_ARGUMENT);
        }
#endif

        StkFloat fInput = (inputL + inputR) * gain_;
        StkFloat outL = 0.0;
        StkFloat outR = 0.0;

        // Parallel LBCF filters
        for (int i = 0; i < nCombs; i++)
        {
            // Left channel
            // StkFloat yn = fInput + (roomSize_ * Freeverb::undenormalize(combLPL_[i].tick(Freeverb::undenormalize(combDelayL_[i].nextOut()))));
            StkFloat yn = fInput + Freeverb::undenormalize(roomSize_ * combLPL_[i].tick(combDelayL_[i].nextOut()));
            combDelayL_[i].tick(yn);
            outL += yn;

            // Right channel
            yn = fInput + (roomSize_ * Freeverb::undenormalize(combLPR_[i].tick(Freeverb::undenormalize(combDelayR_[i].nextOut()))));
            //yn = fInput + (roomSize_ * combLPR_[i].tick(combDelayR_[i].nextOut()));
            combDelayR_[i].tick(yn);
            outR += yn;
        }

        // Series allpass filters
        for (int i = 0; i < nAllpasses; i++)
        {
            // Left channel
            StkFloat vn_m = Freeverb::undenormalize(allPassDelayL_[i].nextOut());
            //StkFloat vn_m = allPassDelayL_[i].nextOut();
            StkFloat vn = outL + (g_ * vn_m);
            allPassDelayL_[i].tick(vn);

            // calculate output
            outL = -vn + (1.0 + g_) * vn_m;

            // Right channel
            vn_m = Freeverb::undenormalize(allPassDelayR_[i].nextOut());
            // vn_m = allPassDelayR_[i].nextOut();
            vn = outR + (g_ * vn_m);
            allPassDelayR_[i].tick(vn);

            // calculate output
            outR = -vn + (1.0 + g_) * vn_m;
        }

        // Mix output
        *pOutL = outL * wet1_ + outR * wet2_ + inputL * dry_;
        *pOutR = outR * wet1_ + outL * wet2_ + inputR * dry_;
    }

}

#endif