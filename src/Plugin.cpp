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

/*

    Plugin.cpp -  C to C++ bindings for Lv2 Plugins.
*/

#include "InputStage.h"
#include "PowerStage2.h"
#include "CabSim.h"
#include "record_plugins/ToobRecordMono.hpp"
#include "ToneStack.h"
#include <lv2_plugin/Lv2Plugin.hpp>
#include <string.h>
#include "lv2/state/state.h"
#include <vector>
#include "SpectrumAnalyzer.h"
#include "ToobML.h"
#include "ToobTuner.h"
#include "ToobFreeverb.h"
#include "ToobDelay.h"
#include "ToobChorus.h"
#include "ToobConvolutionReverb.h"
#include "ToobFlanger.h"
#include "NeuralAmpModeler.h"

using namespace toob;



#define PLUGIN_REGISTER(PLUGIN)  \
    namespace decl_##PLUGIN { \
        REGISTRATION_DECLARATION PluginRegistration<PLUGIN> registration(PLUGIN::URI); \
    }


#define    PLUGIN_REGISTER2(URI,PLUGIN, HAS_STATE)  \
    namespace decl_##PLUGIN { \
        REGISTRATION_DECLARATION PluginRegistration<PLUGIN> registration(URI); \
    }


    PLUGIN_REGISTER(InputStage);
    PLUGIN_REGISTER(PowerStage2);
    PLUGIN_REGISTER(CabSim);
    PLUGIN_REGISTER(ToneStack);
    PLUGIN_REGISTER(SpectrumAnalyzer);
    PLUGIN_REGISTER(ToobML);
    PLUGIN_REGISTER(ToobTuner);
    PLUGIN_REGISTER(ToobFreeverb);
    PLUGIN_REGISTER(ToobDelay);
    PLUGIN_REGISTER(ToobChorus);
    PLUGIN_REGISTER(ToobConvolutionReverbMono); 
    PLUGIN_REGISTER(ToobConvolutionReverbStereo); 
    PLUGIN_REGISTER(ToobConvolutionCabIr); 
    PLUGIN_REGISTER(NeuralAmpModeler);
    PLUGIN_REGISTER(ToobFlanger);
    PLUGIN_REGISTER(ToobFlangerStereo);
    PLUGIN_REGISTER(ToobRecordMono);


int main(void)
{
    return 0;
}

