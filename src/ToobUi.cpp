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

#include "ToobUi.hpp"
#include "AboutDialog.hpp"
#include "lv2c/Lv2cVerticalStackElement.hpp"
#include "lv2c/Lv2cFlexGridElement.hpp"
#include "lv2c/Lv2cSvgElement.hpp"
#include "lv2c/Lv2cButtonElement.hpp"
#include "lv2c/Lv2cScrollContainerElement.hpp"

using namespace lv2c::ui;
using namespace lv2c;
using namespace toob;



ToobUi::ToobUi(
    std::shared_ptr<Lv2PluginInfo> pluginInfo, 
    Lv2cSize defaultWindowSize, 
    Lv2cSize defaultHelpWindowsize,
    const std::string&logoSvg)
: super(pluginInfo,defaultWindowSize)
, defaultHelpWindowSize(defaultHelpWindowsize)
, logoSvg(logoSvg)
{
    // TO-DO: remove the argument.
    defaultHelpWindowSize = Lv2cSize(600,600);
    Lv2cTheme::ptr theme = Lv2cTheme::Create(true); // start with dark theme.
    //theme->paper = Lv2cColor("#081808"); // something dark.
    this->Theme(theme);

}
ToobUi::ToobUi(std::shared_ptr<Lv2PluginInfo> pluginInfo, 
    const Lv2cCreateWindowParameters &createWindowParams,
    Lv2cSize defaultHelpWindowSize,
    const std::string&logoSvg)
: super(pluginInfo,createWindowParams)
, defaultHelpWindowSize(defaultHelpWindowSize)
, logoSvg(logoSvg)
{
    // TO-DO: remove the argument.
    defaultHelpWindowSize = Lv2cSize(600,600);

}



Lv2cContainerElement::ptr ToobUi::RenderBottomBar()
{
    auto bottomBar = Lv2cFlexGridElement::Create();
    bottomBar->Style()
        .BorderWidthTop(1)
        .BorderColor(Lv2cColor("#E0E0E080"))
        .FlexAlignItems(Lv2cAlignment::Center)
        .FlexDirection(Lv2cFlexDirection::Row)
        .FlexWrap(Lv2cFlexWrap::NoWrap)
        .HorizontalAlignment(Lv2cAlignment::Stretch)
        ;
    {
        auto img = Lv2cSvgElement::Create();
        img->Source(logoSvg);
        img->Style()
            .MarginLeft(8)
            .MarginTop(4)
            .MarginBottom(4)
            .Opacity(0.75)
            ;
        bottomBar->AddChild(img);
    }
    {
        auto item = Lv2cElement::Create();
        item->Style()
            .HorizontalAlignment(Lv2cAlignment::Stretch);
        bottomBar->AddChild(item);
    }
    {
        auto button = Lv2cButtonElement::Create();
        button
            ->Variant(Lv2cButtonVariant::ImageButton)
            .Icon("help.svg");
        button->Clicked.AddListener(
            [this](const Lv2cMouseEventArgs&args)
            {
                OnHelpClicked();
                return true;
            }
        );
        bottomBar->AddChild(button);
    }
    return bottomBar;
}

Lv2cContainerElement::ptr ToobUi::Render()
{
    auto container = Lv2cVerticalStackElement::Create();
    container->Style()
        .VerticalAlignment(Lv2cAlignment::Stretch)
        .HorizontalAlignment(Lv2cAlignment::Stretch)
        .Background(Theme()->paper);

    {
        auto controls = RenderClientArea();
        container->AddChild(controls);
    }
    {
        auto bottomBar = RenderBottomBar();
        container->AddChild(bottomBar);
    }
    return container;

}

Lv2cContainerElement::ptr ToobUi::RenderClientArea()
{
    Lv2cScrollContainerElement::ptr scrollElement = Lv2cScrollContainerElement::Create();
    scrollElement->HorizontalScrollEnabled(false)
        .VerticalScrollEnabled(true);
    scrollElement->Style().Background(Theme()->paper).HorizontalAlignment(Lv2cAlignment::Stretch).VerticalAlignment(Lv2cAlignment::Stretch);

    auto controls = RenderControls();
    controls->Style()
        .FlexJustification(Lv2cFlexJustification::Center)
        ;
    scrollElement->Child(controls);
    return scrollElement;

}


void ToobUi::OnHelpClicked()
{
    AboutDialog::ptr dialog = AboutDialog::Create();
    if (this->aboutDialog)
    {
        return;
    }
    dialog->Show(this->Window(),this->defaultHelpWindowSize,this);
    aboutDialog = dialog;
}


void ToobUi::ui_delete()
{
    if (this->aboutDialog)
    {
        aboutDialog->Close();
        aboutDialog = nullptr;
    }
    super::ui_delete();
}


void ToobUi::OnAboutDialogClosed(AboutDialog*dlg)
{
    if (this->aboutDialog.get() == dlg)
    {
        this->aboutDialog = nullptr;
    }
}