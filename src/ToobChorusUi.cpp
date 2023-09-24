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
#include "lvtk/LvtkContainerElement.hpp"
#include "lvtk/LvtkFlexGridElement.hpp"
#include "lvtk/LvtkSvgElement.hpp"
#include "lvtk/LvtkButtonElement.hpp"
#include "ToobUi.hpp"

using namespace lvtk::ui;
using namespace lvtk;
using namespace toob;

// The plugin info generated by generate_lvtk_plugin_info.
#include "ToobChorusInfo.hpp"

#define PLUGIN_CLASS ToobChorusPluginUi
#define PLUGIN_UI_URI "http://two-play.com/plugins/toob-chorus-ui"
#define PLUGIN_INFO_CLASS ToobChorusPluginInfo


 
// class declaration.
class PLUGIN_CLASS: public ToobUi {
public:
    using super=ToobUi;
    using self=PLUGIN_CLASS;
    PLUGIN_CLASS();
protected:
    virtual LvtkContainerElement::ptr RenderClientArea() override;
};


PLUGIN_CLASS::PLUGIN_CLASS() 
: super(
    PLUGIN_INFO_CLASS::Create(),
    LvtkSize(320,200), // default window size.
    "ToobChorusLogo.svg"
    )
{

}



LvtkContainerElement::ptr PLUGIN_CLASS::RenderClientArea() {
    LvtkContainerElement::ptr controlContainer = LvtkContainerElement::Create();
    controlContainer->Style()
        .HorizontalAlignment(LvtkAlignment::Stretch)
        .VerticalAlignment(LvtkAlignment::Stretch)
        ;
    {
        auto controls = super::RenderControls();
        controls->Style()
            .FlexJustification(LvtkFlexJustification::Center)
            .VerticalAlignment(LvtkAlignment::Center)
            ;
        controlContainer->AddChild(controls);
    }
    return controlContainer;
}



// Make the plugin visible to LV2 hosts.

static Lv2UIRegistration<PLUGIN_CLASS> 
registration { PLUGIN_UI_URI};

