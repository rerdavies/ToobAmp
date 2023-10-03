#include "LsNumerics/Freeverb.hpp"

// Just make sure that ToobAmp.so isn't missing any linkages.
// If we built successfully, everything's ok.

#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/ui/ui.h"
#include "lv2/urid/urid.h"

#include "MapFeature.h"

extern void UiLinkageTest();

extern const LV2UI_Descriptor *
lv2ui_descriptor(uint32_t index);

using namespace toob;

class Lv2UiHost {
public:
    void Load(const LV2UI_Descriptor*descriptor);
private:
    std::vector<LV2_Feature*> features;
    MapFeature mapFeature;
    LV2_Feature parentFeature;

    virtual void WriteFunction(
        uint32_t         port_index,
        uint32_t         buffer_size,
        uint32_t         port_protocol,
        const void*      buffer);

    static void write_function(
        LV2UI_Controller controller,
        uint32_t         port_index,
        uint32_t         buffer_size,
        uint32_t         port_protocol,
        const void*      buffer);

};

void Lv2UiHost::WriteFunction(
    uint32_t         port_index,
    uint32_t         buffer_size,
    uint32_t         port_protocol,
    const void*      buffer)
{

}

void Lv2UiHost::write_function(
    LV2UI_Controller controller,
    uint32_t         port_index,
    uint32_t         buffer_size,
    uint32_t         port_protocol,
    const void*      buffer)
{
    Lv2UiHost*pHost = (Lv2UiHost*)controller;
    pHost->WriteFunction(
        port_index,
        buffer_size,
        port_protocol,
        buffer);
}

void Lv2UiHost::Load(const LV2UI_Descriptor *descriptor)
{
    parentFeature.URI = LV2_UI__parent;
    parentFeature.data = nullptr;
    features.push_back((LV2_Feature*)mapFeature.GetFeature());
    features.push_back(&parentFeature);
    features.push_back(nullptr);
    descriptor->instantiate(
        descriptor,
        descriptor->URI,
        ".",
        write_function,
        (LV2UI_Controller)this,
        0,
        &(features[0])
    );

}



extern void*toobChorusLinkage();

int main(int argc, char**argv)
{

    toobChorusLinkage();
    
    const LV2UI_Descriptor *descriptor = lv2ui_descriptor(0);

    //
    (void)descriptor;

    Lv2UiHost host;

    host.Load(descriptor);

    return 0;
}