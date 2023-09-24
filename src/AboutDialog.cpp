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
#include "lvtk/LvtkTypographyElement.hpp"
#include "lvtk/LvtkVerticalStackElement.hpp"
#include "lvtk/LvtkScrollContainerElement.hpp"

using namespace toob;
using namespace lvtk;


void AboutDialog::Show(
    LvtkWindow::ptr parent,
    const std::string&settingsKey,
    const std::string&title,
    const std::string&text)
{
    Theme(parent->ThemePtr());

    this->text = text;
    this->title = title;
    LvtkCreateWindowParameters windowParameters;

    windowParameters.backgroundColor = Theme().paper;
    windowParameters.positioning = LvtkWindowPositioning::CenterOnParent;
    windowParameters.title = title;
    windowParameters.settingsKey = settingsKey;
    windowParameters.windowType = LvtkWindowType::Dialog;

    windowParameters.settingsObject = parent->WindowParameters().settingsObject;

    windowParameters.owner = parent.get();

    super::CreateChildWindow(parent.get(),windowParameters,Render(text));
    
}

WindowHandle AboutDialog::GetApplicationWindow(LvtkWindow::ptr parent)
{
    return parent->Handle();
}

static std::vector<std::string> getLines(const std::string&text)
{
    std::vector<std::string> result;
    std::stringstream s {text};
    while (s)
    {
        std::string line;
        std::getline(s,line);
        result.push_back(std::move(line));
    }
    return result;
}

static LvtkTypographyElement::ptr MarkupLine(const std::string&text)
{
    auto typography = LvtkTypographyElement::Create();
    typography->Variant(LvtkTypographyVariant::BodyPrimary)
        .Text(text)
        ;
    typography->Style()
        .MarginBottom(16)
        .SingleLine(false)
        ;
    return typography;
}

static LvtkElement::ptr Markup(const std::string &text)
{
    std::vector<LvtkElement::ptr> paragraphs;
    std::vector<std::string> lines = getLines(text);
    std::string lineBuffer;
    for (std::string&line: lines)
    {
        if (line.length() == 0)
        {
            if (lineBuffer.length() != 0)
            {
                paragraphs.push_back(MarkupLine(lineBuffer));
                lineBuffer.resize(0);
            }
        } else {
            lineBuffer += line;
        }
    }
    if (lineBuffer.length() != 0)
    {
        paragraphs.push_back(MarkupLine(lineBuffer));
        lineBuffer.resize(0);
    }

    LvtkVerticalStackElement::ptr stack = LvtkVerticalStackElement::Create();
    stack->Style()
        .Padding({24,16,24,16})
        .HorizontalAlignment(LvtkAlignment::Stretch)
        ;
    stack->Children(paragraphs);
    return stack;
}

LvtkElement::ptr AboutDialog::Render(const std::string&text)
{

    LvtkScrollContainerElement::ptr scrollContainer = LvtkScrollContainerElement::Create();
        scrollContainer->Style()
        .HorizontalAlignment(LvtkAlignment::Stretch)
        .VerticalAlignment(LvtkAlignment::Stretch)
        ;

    scrollContainer->Child(Markup(text));
    return scrollContainer;

}
