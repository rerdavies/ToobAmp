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
#include "lvtk/LvtkVerticalStackElement.hpp"
#include "lvtk/LvtkFlexGridElement.hpp"
#include "lvtk/LvtkSvgElement.hpp"
#include "lvtk/LvtkButtonElement.hpp"
#include "lvtk/LvtkScrollContainerElement.hpp"

using namespace lvtk::ui;
using namespace lvtk;
using namespace toob;



ToobUi::ToobUi(
    std::shared_ptr<Lv2PluginInfo> pluginInfo, 
    LvtkSize defaultWindowSize, 
    LvtkSize defaultHelpWindowsize,
    const std::string&logoSvg)
: super(pluginInfo,defaultWindowSize)
, defaultHelpWindowSize(defaultHelpWindowsize)
, logoSvg(logoSvg)
{
    // TO-DO: remove the argument.
    defaultHelpWindowSize = LvtkSize(600,600);
}
ToobUi::ToobUi(std::shared_ptr<Lv2PluginInfo> pluginInfo, 
    const LvtkCreateWindowParameters &createWindowParams,
    LvtkSize defaultHelpWindowSize,
    const std::string&logoSvg)
: super(pluginInfo,createWindowParams)
, defaultHelpWindowSize(defaultHelpWindowSize)
, logoSvg(logoSvg)
{
    // TO-DO: remove the argument.
    defaultHelpWindowSize = LvtkSize(600,600);

}



LvtkContainerElement::ptr ToobUi::RenderBottomBar()
{
    auto bottomBar = LvtkFlexGridElement::Create();
    bottomBar->Style()
        .BorderWidthTop(1)
        .BorderColor(LvtkColor("#E0E0E080"))
        .FlexAlignItems(LvtkAlignment::Center)
        .FlexDirection(LvtkFlexDirection::Row)
        .FlexWrap(LvtkFlexWrap::NoWrap)
        .HorizontalAlignment(LvtkAlignment::Stretch)
        ;
    {
        auto img = LvtkSvgElement::Create();
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
        auto item = LvtkElement::Create();
        item->Style()
            .HorizontalAlignment(LvtkAlignment::Stretch);
        bottomBar->AddChild(item);
    }
    {
        auto button = LvtkButtonElement::Create();
        button
            ->Variant(LvtkButtonVariant::ImageButton)
            .Icon("help.svg");
        button->Clicked.AddListener(
            [this](const LvtkMouseEventArgs&args)
            {
                OnHelpClicked();
                return true;
            }
        );
        bottomBar->AddChild(button);
    }
    return bottomBar;
}

LvtkContainerElement::ptr ToobUi::Render()
{
    auto container = LvtkVerticalStackElement::Create();
    container->Style()
        .VerticalAlignment(LvtkAlignment::Stretch)
        .HorizontalAlignment(LvtkAlignment::Stretch)
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

LvtkContainerElement::ptr ToobUi::RenderClientArea()
{
    LvtkScrollContainerElement::ptr scrollElement = LvtkScrollContainerElement::Create();
    scrollElement->HorizontalScrollEnabled(false)
        .VerticalScrollEnabled(true);
    scrollElement->Style().Background(Theme()->paper).HorizontalAlignment(LvtkAlignment::Stretch).VerticalAlignment(LvtkAlignment::Stretch);

    scrollElement->Child(RenderControls());
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