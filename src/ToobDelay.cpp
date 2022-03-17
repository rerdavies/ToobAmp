/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:

 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.

 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */
#include "ToobDelay.h"

using namespace TwoPlay;

const float MAX_DELAY_MS = 4000;
const float NOMINAL_DELAY_MS = 1600;

ToobDelay::ToobDelay(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2Plugin(features),
      rate(rate),
      bundle_path(bundle_path)

{
}

const char *ToobDelay::URI = TOOB_DELAY_URI;

void ToobDelay::ConnectPort(uint32_t port, void *data)
{
    switch ((PortId)port)
    {
    case PortId::DELAY:
        this->delay = (float *)data;
        break;
    case PortId::LEVEL:
        this->level = (float *)data;
        break;
    case PortId::FEEDBACK:
        this->feedback = (float *)data;
        break;
    case PortId::AUDIO_INL:
        this->inL = (const float *)data;
        break;
    case PortId::AUDIO_OUTL:
        this->outL = (float *)data;
        break;
    }
}
void ToobDelay::clear()
{
    for (size_t i = 0; i < delayLine.size(); ++i)
    {
        delayLine[i] = 0;
    }
    this->delayIndex = 0;
}
inline void ToobDelay::updateControls()
{
    if (lastDelay != *delay)
    {
        lastDelay = *delay;
        double t = lastDelay;
        if (t < 0) t = 0;
        if (t > MAX_DELAY_MS)
        {
            t = MAX_DELAY_MS;
        }
        delayValue = (uint32_t)(t * rate / 1000);
        if (delayValue == 0)
            delayValue = 1;

        size_t len = delayValue + 2;
        if (len > delayLine.size())
        {
            delayLine.resize(len);
        }
    }
    if (lastLevel != *level)
    {
        lastLevel = *level;
        double levelValue = lastLevel * 0.01;
        if (levelValue > 1)
        {
            levelValue = 1;
        } else if (levelValue < -1)
        {
            levelValue = -1;
        }
        // "power-ish". But gives a more useful range of values..
        this->levelValue = levelValue*levelValue;
    }
    if (lastFeedback != *feedback)
    {
        lastFeedback = *feedback;
        double feedbackValue = (lastFeedback)*0.01;
        if (feedbackValue > 0.999)
        {
            feedbackValue = 0.999;
        } else if (feedbackValue < -0.999) {
            feedbackValue = -0.999;
        }
        // "power-ish". But gives a more useful range of values..
        this->feedbackValue = feedbackValue*feedbackValue;
    }
}
void ToobDelay::Activate()
{
    delayLine.resize(uint32_t(NOMINAL_DELAY_MS*rate/1000)+2);
    lastDelay =lastLevel = lastFeedback = -1E30; // force updates
    updateControls();
    clear();
}

void ToobDelay::Run(uint32_t n_samples)
{
    updateControls();
    for (uint32_t i = 0; i < n_samples; ++i)
    {
        float input = inL[i];
        float t = delayGet();
        const float DENORM_GUARD = 1E-11;
        delayPut(input + t*feedbackValue + DENORM_GUARD);
        float output = input + levelValue*t;
        outL[i] = output;
    }
}
void ToobDelay::Deactivate()
{
}
