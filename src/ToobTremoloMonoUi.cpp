// Copyright (c) 2025 Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



#include "ToobTremoloMonoInfo.hpp"
#include "lv2c_ui/Lv2UI.hpp"
#include "ToobUi.hpp"

using namespace lv2c::ui;
using namespace lv2c;
using namespace tremolo_plugin;
using namespace toob;

class ToobTremoloMonoUi: public ToobUi {
public:
    using super=ToobUi;
    ToobTremoloMonoUi();
};



ToobTremoloMonoUi::ToobTremoloMonoUi() : super(
    ToobTremoloMonoUiBase::Create(),
    Lv2cSize(500,223), // default window size.
    Lv2cSize(500,223), // default window size.)
    "ToobTremoloMonoLogo.svg"
    )
{
}

// Refereence this variable to get the linker to demand-And tlink the entire .obj.

REGISTRATION_DECLARATION Lv2UIRegistration<ToobTremoloMonoUi> toobTremoloMonoUiRegistration { ToobTremoloMonoUiBase::UI_URI};





