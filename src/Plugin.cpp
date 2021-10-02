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

