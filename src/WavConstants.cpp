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
#include "WavConstants.hpp"

using namespace toob;

ChannelMask toob::GetChannel(size_t channel, ChannelMask channelMask)
{
    uint32_t mask = (uint32_t)channelMask;
    uint32_t result = 1;

    while (result <= (uint32_t)ChannelMask::SPEAKER_TOP_BACK_RIGHT)
    {
        if ((mask & result) != 0)
        {
            if (channel == 0)
            {
                return (ChannelMask)result;
            }
            --channel;
        }
        result <<= 1;
    }
    throw std::logic_error("Channel mask does not match number of channels.");
}
