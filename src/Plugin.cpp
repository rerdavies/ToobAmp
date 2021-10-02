/*
 *   Copyright (c) 2021 Robin E. R. Davies
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
#include "PowerStage.h"
#include "CabSim.h"
#include "ToneStack.h"
#include "Lv2Plugin.h"
#include <string.h>
#include "lv2/state/state.h"
#include <vector>

using namespace TwoPlay;


std::vector<Lv2PluginFactory> factories = {
    { InputStage::URI, InputStage::Create},
    { PowerStage::URI, PowerStage::Create},
    { CabSim::URI, CabSim::Create},
    { ToneStack::URI, ToneStack::Create},
};

static const LV2_Descriptor*const* descriptors;

static bool initialized;

extern "C" {
    LV2_SYMBOL_EXPORT
        const LV2_Descriptor*
        lv2_descriptor(uint32_t index)
    {
        if (!initialized)
        {
            initialized = true;
            descriptors = Lv2Plugin::CreateDescriptors(factories);

        }
        if (index < factories.size())
        {
            return descriptors[index];
        }
        return NULL;
    }
}

int main(void)
{
    return 0;
}

