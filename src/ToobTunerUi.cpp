/*
Copyright (c) 2023 Robin E. R. Davies

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

#include "lvtk_ui/Lv2UI.hpp"
#include "ToobUi.hpp"
#include "lvtk_ui/Lv2TunerElement.hpp"

using namespace lvtk::ui;
using namespace lvtk;
using namespace toob;

// The plugin info generated by generate_lvtk_plugin_info.
#include "ToobTunerInfo.hpp"


#define PLUGIN_CLASS ToobTunerUi
#define PLUGIN_UI_URI "http://two-play.com/plugins/toob-tuner-ui"
#define PLUGIN_INFO_CLASS ToobTunerInfo

// class declaration.
class PLUGIN_CLASS: public ToobUi {
public:
    using super=ToobUi;
    using self=PLUGIN_CLASS;
    PLUGIN_CLASS();
protected:
    virtual LvtkElement::ptr RenderControl(LvtkBindingProperty<double> &value, const Lv2PortInfo &portInfo) override;
};


PLUGIN_CLASS::PLUGIN_CLASS() 
: super(
    PLUGIN_INFO_CLASS::Create(),
    LvtkSize(527,208), // default window size.
    LvtkSize(470,800),
    "ToobTunerLogo.svg"
    )
{
 
}


LvtkElement::ptr PLUGIN_CLASS::RenderControl(LvtkBindingProperty<double> &value, const Lv2PortInfo &portInfo) 
{
    // Interecept the tuner control so that we bind the reference frequency.
    if (portInfo.symbol() == "FREQ")
    {
        auto tunerControl = Lv2TunerElement::Create();
        value.Bind(tunerControl->ValueProperty);
        auto &referenceFrequencyProperty = this->GetControlProperty("REFFREQ");
        tunerControl->ValueIsMidiNote(true);
        referenceFrequencyProperty.Bind(tunerControl->ReferenceFrequencyProperty);
        return tunerControl;
    } else {
        return super::RenderControl(value,portInfo);
    }
}

// Make the plugin visible to LV2 hosts.

Lv2UIRegistration<PLUGIN_CLASS> 
toobTunerRegistration { PLUGIN_UI_URI};

