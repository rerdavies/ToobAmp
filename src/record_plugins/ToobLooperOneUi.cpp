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


#include "ToobLooperOneInfo.hpp"
#include "lv2c_ui/Lv2UI.hpp"
#include "../ToobUi.hpp"

using namespace lv2c::ui;
using namespace lv2c;
using namespace record_plugin;
using namespace toob;

// The plugin info generated by generate_lv2c_plugin_info.

// class declaration.
class ToobLooperOneUiPlugin: public ToobUi {
public:
    using super=ToobUi;
    ToobLooperOneUiPlugin();
};


ToobLooperOneUiPlugin::ToobLooperOneUiPlugin() 
: super(
    ToobLooperOneUiBase::Create(),
    Lv2cSize(623,409),
    Lv2cSize(623,409),
    "LooperOneLogo.svg"
    )
{

    Lv2cTheme::ptr theme = Lv2cTheme::Create(true); // start with dark theme.
    this->Theme(theme);

}


// Make the plugin visible to LV2 hosts.



REGISTRATION_DECLARATION Lv2UIRegistration<ToobLooperOneUiPlugin> 
toobLooperOneUiRegistration { ToobLooperOneUiBase::UI_URI  };

