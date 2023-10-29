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

#include "AboutDialog.hpp"
#include "lv2c/Lv2cTypographyElement.hpp"
#include "lv2c/Lv2cVerticalStackElement.hpp"
#include "lv2c/Lv2cTableElement.hpp"
#include "lv2c/Lv2cScrollContainerElement.hpp"
#include "lv2c_ui/Lv2PluginInfo.hpp"
#include "ToobUi.hpp"
#include <fstream>
#include "lv2c/Lv2cMarkdownElement.hpp"
#include "ToobAmpVersion.hpp"

using namespace toob;
using namespace lvtk;
using namespace lvtk::ui;

void AboutDialog::Show(
    Lv2cWindow::ptr parent,
    Lv2cSize defaultDialogSize,
    ToobUi *toobUi)
{
    this->toobUi = toobUi;
    Theme(parent->ThemePtr());

    const Lv2PluginInfo &pluginInfo = toobUi->PluginInfo();

    std::string settingsKey = "dlg-" + pluginInfo.uri();
    std::string title = "Help - " + pluginInfo.name();

    Lv2cCreateWindowParameters windowParameters;

    windowParameters.backgroundColor = Theme().popupBackground;
    windowParameters.positioning = Lv2cWindowPositioning::CenterOnParent;
    windowParameters.title = title;
    windowParameters.settingsKey = settingsKey;
    windowParameters.windowType = Lv2cWindowType::Utility;
    windowParameters.minSize = Lv2cSize(320, 200);
    windowParameters.maxSize = Lv2cSize(10000, 10000);
    windowParameters.size = defaultDialogSize;
    windowParameters.x11Windowclass = "com.twoplay.lvtk-plugin"; // Maybe used for settings by Window Managers.
    windowParameters.gtkApplicationId = windowParameters.x11Windowclass;
    windowParameters.x11WindowName = title;

    windowParameters.settingsObject = parent->WindowParameters().settingsObject;

    windowParameters.owner = parent.get();

    super::CreateChildWindow(parent.get(), windowParameters, Render(pluginInfo));
    scrollContainer->Focus();
}

WindowHandle AboutDialog::GetApplicationWindow(Lv2cWindow::ptr parent)
{
    return parent->Handle();
}

Lv2cElement::ptr AboutDialog::RenderDivider()
{
    auto element = Lv2cElement::Create();
    element->Style()
        .Height(1)
        .HorizontalAlignment(Lv2cAlignment::Stretch)
        .Background(Theme().dividerColor)
        .MarginTop({4})
        .MarginBottom({8});
    return element;
}

Lv2cVerticalStackElement::ptr AboutDialog::Markup(const std::string &text)
{
    auto element = Lv2cMarkdownElement::Create();
    element->SetMarkdown(text);
    return element;
}

static bool HasControlDocs(const Lv2PluginInfo &pluginInfo)
{
    for (auto &port : pluginInfo.ports())
    {
        if (port.is_control_port() && port.is_input() && port.comment().length() != 0)
        {
            return true;
        }
    }
    return false;
}

Lv2cElement::ptr AboutDialog::RenderPortDocs(const Lv2PluginInfo &pluginInfo)
{
    Lv2cTableElement::ptr table = Lv2cTableElement::Create();
    table->Style()
        .HorizontalAlignment(Lv2cAlignment::Stretch)
        .MarginBottom(16);
    table->ColumnDefinitions(
        {
            {
                Lv2cAlignment::Start,
                Lv2cAlignment::Start,
                0,
            },
            {
                Lv2cAlignment::Start,
                Lv2cAlignment::Stretch,
                1,
            },
        });
    table->Style()
        .CellPadding({4})
        .BorderWidth({1})
        .BorderColor(Theme().dividerColor);
    for (auto &port : pluginInfo.ports())
    {
        auto comment = port.comment();
        if (port.is_control_port() && port.is_input() && comment.length() && !port.not_on_gui())
        {
            Lv2cTypographyElement::ptr nameElement = Lv2cTypographyElement::Create();
            nameElement->Variant(Lv2cTypographyVariant::BodyPrimary)
                .Text(port.name());
            nameElement->Style().SingleLine(true);

            auto textElement = Markup(comment);
            // remove bottom padding from the last typographyElement.
            if (textElement->ChildCount() > 0)
            {
                auto lastParagraph = textElement->Children()[textElement->ChildCount() - 1];
                lastParagraph->Style().PaddingBottom({4.0});
                lastParagraph->Style().MarginBottom({0.0});
            }

            table->AddRow({nameElement, textElement});
        }
    }

    return table;
}

Lv2cElement::ptr AboutDialog::Render(const Lv2PluginInfo &pluginInfo)
{

    scrollContainer = Lv2cScrollContainerElement::Create();
    scrollContainer->WantsFocus(true);
    scrollContainer->Style()
        .HorizontalAlignment(Lv2cAlignment::Stretch)
        .VerticalAlignment(Lv2cAlignment::Stretch)
        .Background(Theme().popupBackground);

    {
        primaryText = true;
        Lv2cVerticalStackElement::ptr textContainer = Lv2cVerticalStackElement::Create();
        textContainer->Style()
            .HorizontalAlignment(Lv2cAlignment::Stretch)
            .Margin({32, 16, 32, 16});

        if (HasControlDocs(pluginInfo))
        {
            textContainer->AddChild(RenderPortDocs(pluginInfo));
        }
        {
            auto element = Lv2cMarkdownElement::Create();
            element->SetMarkdown(pluginInfo.comment());
            textContainer->AddChild(element);
        }
        {
            // spacer above copyrights.
            auto element = Lv2cElement::Create();
            element->Style().Height(24);
            textContainer->AddChild(element);
        }
        primaryText = false;
        textContainer->AddChild(RenderLicenses());
        scrollContainer->Child(textContainer);
    }
    return scrollContainer;
}

Lv2cElement::ptr AboutDialog::RenderLicenses()
{
    auto textContainer = Lv2cVerticalStackElement::Create();
    {
        textContainer->AddChild(RenderDivider());
        {
            Lv2cTypographyElement::ptr typography = Lv2cTypographyElement::Create();
            typography->Variant(Lv2cTypographyVariant::BodySecondary)
                .Text("TooB LV2 Guitar Effects v" TOOBAMP_BUILD_LABEL);
            typography->Style()
                .MarginTop(16)
                .MarginBottom(16)
                ;
            textContainer->AddChild(typography);
        }
        {
            auto element = Lv2cMarkdownElement::Create();
            element->TextVariant(Lv2cTypographyVariant::BodySecondary);
            
            element->AddMarkdownFile(std::filesystem::path(this->toobUi->BundlePath()) / "LICENSE.md");
            textContainer->AddChild(element);
        }
    }
    return textContainer;
}

void AboutDialog::OnClosing()
{
    if (toobUi)
    {
        toobUi->OnAboutDialogClosed(this);
        toobUi = nullptr;
    }
}