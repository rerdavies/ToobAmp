/*
Copyright (c) 2024 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include "ss.hpp"
#include "Finally.hpp"
#include <stdexcept>
#include "util.hpp"

using namespace std;
using namespace pipedal;

namespace fs = std::filesystem;

const std::string version = "8";
const std::string title = "1.1.50 Release";

static const float PROFILE_TIME = 40;


/*
./NeuralAmpModels/Fender Twin Pack/Tim R Fender TwinVerb Vibrato Bright.nam
./NeuralAmpModels/Fender Twin Pack/Tim R Fender TwinVerb Norm Bright.nam
./NeuralAmpModels/Fender HotRod Deluxe v3 pack/ch2 moreagain.nam
./NeuralAmpModels/Fender HotRod Deluxe v3 pack/ch2 highgain.nam
./NeuralAmpModels/Fender HotRod Deluxe v3 pack/ch1 bright.nam
./NeuralAmpModels/Fender HotRod Deluxe v3 pack/ch1.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 01.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 02.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 00C No Cabs.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 06.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 08.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 11.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 04.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 12.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 09.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 13.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 10.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 00R  no Cabs.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 07.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 05.nam
./NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 03.nam
./NeuralAmpModels/Peavey 6505MH Simple High Gain Pack/6505MH G1 Flat.nam
./NeuralAmpModels/Peavey 6505MH Simple High Gain Pack/Cc/6505MH G1 Boost.nam
./NeuralAmpModels/Peavey 6505MH Simple High Gain Pack/6505MH G2 Flat.nam
./NeuralAmpModels/Peavey 6505MH Simple High Gain Pack/6505MH G1 Scoop.nam
./NeuralAmpModels/Peavey 6505MH Simple High Gain Pack/6505MH G2 Scoop.nam
./NeuralAmpModels/Peavey 6505MH Simple High Gain Pack/6505MH G2 Boost.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G9.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G9.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G9.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G9.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G9.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G4.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G8.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G8.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G3.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G4.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G6.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G9.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G9.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G7.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep On - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G9.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G1.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G3.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright Off - G10.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D0 - B1 - G2.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - BO - G2.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G6.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Normal Channel - Bright On - G7.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G5.5.nam
./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - D1 - B1 - G9.5.nam
./NeuralAmpModels/Fender Hotrod Deluxe Pushed Clean_Breakup NANO!/Fender Hotrod Deluxe Clean BreakupPerfect Nano  .nam
./neural_amp_modeler.lv2/6505MH G1 Boost.nam
./neural_amp_modeler.lv2/6505MH G1 Flat.nam
./neural_amp_modeler.lv2/6505MH G2 Flat.nam
./neural_amp_modeler.lv2/6505MH G2 Scoop xx.namls 
./neural_amp_modeler.lv2/6505MH G1 Scoop.nam
./neural_amp_modeler.lv2/6505MH G2 Boost.nam
*/

static std::vector<std::string> testModels{
    "NeuralAmpModels/Fender Twin Pack/Tim R Fender TwinVerb Vibrato Bright.nam",
    "./NeuralAmpModels/Fender Bassman 50 (0.5.2)/FENDER BASSMAN 50 - JUMPED - DO - BO - G10.nam",
    "NeuralAmpModels/Tone King Imperial Mk 11 - 15 Feather Captures/Tone King Imperial Mk 11 - 01.nam",
    "NeuralAmpModels/Fender Hotrod Deluxe Pushed Clean_Breakup NANO!/Fender Hotrod Deluxe Clean BreakupPerfect Nano  .nam",

};

static std::string ReadWholeFile(const fs::path &path)
{
    std::ifstream t(path);
    if (!t.is_open())
    {
        throw std::runtime_error(SS("Failed to open " << fs::absolute(path)));
    }
    std::stringstream ss;
    ss << t.rdbuf();
    return ss.str();
}

void WritePreset(const std::string &model, const fs::path &templatePath, const fs::path &targetFilename)
{
    const std::string match = "NeuralAmpModels/Fender Bassman 50 (0.5.2)/Fender Bassman 50 - Bass Channel - Deep Off - G1.nam";
    std::string result = ReadWholeFile(fs::absolute(templatePath));

    size_t pos = 0;
    size_t replacements = 0;
    while (true)
    {
        auto nextPos = result.find(match, pos);
        if (nextPos == string::npos)
        {
            break;
        }
        ++replacements;
        result = result.substr(0, nextPos) + model + result.substr(nextPos + match.length());
        pos = nextPos + model.length();
    }
    if (replacements != 2) 
    {
        throw std::runtime_error("Failed update the preset model.");
    }
    std::ofstream f(targetFilename);
    if (!f.is_open())
    {
        cout << "Error: Can't open " << targetFilename << endl;
        exit(EXIT_FAILURE);
    }
    f << result;
}

static void ProfileModel(const std::string &model, ostream *outputFile)
{
    cout << "Model: \"" << model << "\"" << endl;
    if (outputFile)
    {
        (*outputFile) << "Model: \"" << model << "\"" << endl;
    }
    fs::path profileCmd = "/usr/bin/pipedalProfilePlugin";
    if (!fs::exists(profileCmd))
    {
        throw std::runtime_error(SS(profileCmd << " not found. Build the rerdavies/pipedal project, and run ./install.sh"));
    }
    WritePreset(model, "./Toob Nam.preset", "/tmp/namPreset.preset");


    std::string tmpName = toob::TemporaryFilename();
    Finally ff { [tmpName]() {
        fs::remove(tmpName);
    }};

    std::string cmdline = SS(profileCmd.string() << " --preset-file /tmp/namPreset.preset -w -s " << PROFILE_TIME << " >" << tmpName);
    auto result = std::system(cmdline.c_str());

    std::string output = ReadWholeFile(tmpName);

    cout << output;

    if (*outputFile)
    {
        (*outputFile) << output;
    }

    if (result != EXIT_SUCCESS)
    {
        throw std::runtime_error("pipedalProfilePlugin failed.");
    }
}

int main(int argc, char **argv)
{

    try
    {

        fs::path historyFilePath = fs::absolute(fs::path("history"));
        fs::create_directories(historyFilePath);

        historyFilePath /= SS("ProfileResults_" << version << ".txt");
        ofstream history(historyFilePath);
        history << version << " - " << title << " - " << PROFILE_TIME << "s" << endl;;

        for (const auto &testModel : testModels)
        {
            ProfileModel(testModel, &history);
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
