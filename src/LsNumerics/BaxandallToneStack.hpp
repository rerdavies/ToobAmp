#pragma once

// based on ampbooks.com/mobile/dsp/tonestack

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


#include "InPlaceBilinearFilter.h"
#include "LsMath.hpp"

namespace LsNumerics {

/*
    BaxandallToneStack - Digital emulation of the Baxandall/James Analog Tone Stack.

    Call  SetSampleRate preferrably at initialization time, and then call Design(bass,treble) to initialize the filter. The 
    Design call makes no heap allocations, and may be called on a realtime thread).

    bass and treble values must be between zero and one, with 0.5 being flat response (+/- 3db from 0hz to sampleRate/2).

    bass and treble controls provide very approximately a +/-15 db shelved boost or cut, while the center frequency response
    at about 300Hz remains close to zero Db (i.e. it emulates an active Baxandall/James analog tone stack).

    Derived from heavy math, executed by Richard Kuehel [1].

            "We exhaustively worked out the equations for all the tone stacks by evaluating 
            the mesh and node equations. Fortunately we have computers that handled the polynomial
            reduction but it was still a Herculean effort." [2] -Fractal Audio Systems


    ------------------------

    [1] "Digital Modelling of a Guitar Amplifier Tone Stack", Richard Kuehel,
    http://ampbooks.com/mobile/dsp/tonestack, Retreived Feb 16, 2022.

    [2] Fractal Audio Systems, Multipoint Iterative Matching and Impedance 
    Correction Technology (MIMICTM), April 2013, p. 7.

*/
class BaxandallToneStack: public InPlaceBilinearFilter<5> {
public:
    constexpr static double DefaultMakeupGain = 17.41;

    void SetSampleRate(double sampleRate)
    {
        this->InitTransform(sampleRate,Fc,Fc);
    }
    /// Sets the makeup gain for the passive Baxandall network. The default value (22db) produces
    // roughly zero Db gain with treble and bass dials at 0.5.
    void SetActiveGainDb(double activeGainDb)
    {
        this->activeGain = activeGainDb;
        this->activeGainFactor = Db2Af(activeGainDb);
        totalGain = midGainFactor*activeGainFactor;
    }
    // Get the current makeup gain (see ActiveGainDb(double))
    double SetActiveGainDb() const { return activeGain; }
    void Design(double bass, double treble);
    void Design(double bass, double mid, double treble);

    double Tick(double value) {
        return this->InPlaceBilinearFilter::Tick(value)*totalGain;
    }

    static constexpr double Fc = 300;
    double GetFrequencyResponse(double frequency)
    {
        return this->InPlaceBilinearFilter::GetFrequencyResponse(frequency)*totalGain;
    }
    // frequency response of the analog prototype (for testing)
    double GetDesignFrequencyResponse(double frequency);

private:
    double activeGain = DefaultMakeupGain; // in db.
    double activeGainFactor = Db2Af(DefaultMakeupGain);
    double midGainFactor = 1;
    double totalGain = activeGainFactor;
    double a[5];
    double b[5];

};

} // namespace