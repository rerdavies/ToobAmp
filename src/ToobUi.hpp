// Copyright (c) 2023 Robin Davies
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
#pragma once

#include "lvtk_ui/Lv2UI.hpp"

namespace toob {
    using namespace lvtk::ui;
    using namespace lvtk;

    class AboutDialog;

    class ToobUi: public lvtk::ui::Lv2UI {
    public:
        using super = Lv2UI;
        using self = ToobUi;
        

        ToobUi(std::shared_ptr<Lv2PluginInfo> pluginInfo, LvtkSize defaultWindowSize, const std::string&logoSvg);
    protected:
        virtual LvtkContainerElement::ptr RenderClientArea();
        virtual LvtkContainerElement::ptr RenderBottomBar();

        virtual LvtkContainerElement::ptr Render() override;
    
        virtual void OnHelpClicked();

        virtual void ui_delete() override;

    private:
        std::shared_ptr<AboutDialog> aboutDialog;
        std::string logoSvg;
    };
}