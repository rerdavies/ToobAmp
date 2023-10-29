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
#pragma once

#include "lv2c/Lv2cWindow.hpp"
#include "lv2c/Lv2cVerticalStackElement.hpp"
#include "lv2c_ui/Lv2PluginInfo.hpp"

namespace lvtk {
    class Lv2cScrollContainerElement;

}
namespace toob {
    using namespace lvtk;
    class ToobUi;

    class AboutDialog : public Lv2cWindow {
    public:
        using self=AboutDialog;
        using super=Lv2cWindow;
        using ptr = std::shared_ptr<self>;
        static ptr Create() { return std::make_shared<AboutDialog>(); }

        void Show(
            Lv2cWindow::ptr parent,
            Lv2cSize defaultDialogSize,
            ToobUi*toobUi);

    protected:
        virtual void OnClosing() override;

    private:
        std::shared_ptr<Lv2cScrollContainerElement> scrollContainer;
        bool primaryText = true;
        Lv2cElement::ptr RenderDivider();
        Lv2cElement::ptr RenderLicenses();
        Lv2cVerticalStackElement::ptr Markup(const std::string &text);    
        Lv2cElement::ptr Render(const lvtk::ui::Lv2PluginInfo&pluginInfo);
        Lv2cElement::ptr RenderPortDocs(const lvtk::ui::Lv2PluginInfo&pluginInfo);
        ToobUi*toobUi = nullptr;

        WindowHandle GetApplicationWindow(Lv2cWindow::ptr parent);
        std::string settingsKey;
    };
}