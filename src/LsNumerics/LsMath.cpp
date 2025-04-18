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

#include "LsMath.hpp"
#include <sstream>
#include <iomanip>


using namespace LsNumerics;


// float MathInternal::log10 = std::log(10.0f);

uint32_t LsNumerics::NextPowerOfTwo(uint32_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value+1;
}


// Function to convert MIDI note number to note name
std::string LsNumerics::MidiNoteToName(int midiNote)
{
    if (midiNote < 0 || midiNote > 127)
        return "Invalid";

    const std::string noteNames[] = {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    int octave = midiNote / 12 - 1;
    int noteIndex = midiNote % 12;
    return noteNames[noteIndex] + std::to_string(octave);
}

// Function to convert frequency to note name with cents
std::string LsNumerics::FrequencyToNoteName(double freq)
{
    double midiNoteExact = FrequencyToMidiNote(freq);
    if (midiNoteExact < 0)
        return "Invalid";

    int midiNote = std::round(midiNoteExact);
    double cents = 100 * (midiNoteExact - midiNote); // Cents deviation

    std::ostringstream oss;
    oss << LsNumerics::MidiNoteToName(midiNote);
    // Only append cents if non-zero (avoid unnecessary +0.00)
    if (std::abs(cents) > 0.01)
    { // Threshold for floating-point noise
        oss << (cents >= 0 ? "+" : "") << std::fixed << std::setprecision(2) << cents;
    }
    return oss.str();
}

