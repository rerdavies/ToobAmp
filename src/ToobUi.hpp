// Copyright (c) 2023 Robin E. R. Davies
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

#include "lv2c_ui/Lv2UI.hpp"

namespace toob {
    using namespace lv2c::ui;
    using namespace lv2c;

    class AboutDialog;

    class ToobUi: public lv2c::ui::Lv2UI {
    public:
        using super = Lv2UI;
        using self = ToobUi;
        

        ToobUi(std::shared_ptr<Lv2PluginInfo> pluginInfo, 
            Lv2cSize defaultWindowSize,
            Lv2cSize defaultHelpWindowSize,
            const std::string&logoSvg);

        ToobUi(std::shared_ptr<Lv2PluginInfo> pluginInfo, 
            const Lv2cCreateWindowParameters &createWindowParames,
            Lv2cSize defaultHelpWindowSize,
            const std::string&logoSvg);

        void OnAboutDialogClosed(AboutDialog*dlg);
    protected:
        virtual Lv2cContainerElement::ptr RenderClientArea();
        virtual Lv2cContainerElement::ptr RenderBottomBar();

        virtual Lv2cContainerElement::ptr Render() override;
    
        virtual void OnHelpClicked();

        virtual void ui_delete() override;


    private:
        Lv2cSize defaultHelpWindowSize;
        std::shared_ptr<AboutDialog> aboutDialog;
        std::string logoSvg;
    };
}