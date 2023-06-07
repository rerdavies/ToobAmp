/*
 Copyright (c) 2023 Robin Davies

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
#include <cstddef>
#include "WavGuid.hpp"
#include <stdexcept>

namespace toob
{

    // Redeclaration of Windows types.
    namespace private_use
    {

        enum class ChunkIds
        {
            Riff = 0x46464952,
            WaveRiff = 0x45564157,
            Format = 0x020746d66,
            Data = 0x61746164,

        };

        enum class WavFormat
        {
            PulseCodeModulation = 0x01,
            IEEEFloatingPoint = 0x03,
            Extensible = 0xFFFE
        };

        struct WaveFormatExtensible
        {
            uint16_t wFormatTag;
            uint16_t nChannels;
            uint32_t nSamplesPerSec;
            uint32_t nAvgBytesPerSec;
            uint16_t nBlockAlign;
            uint16_t wBitsPerSample;
            uint16_t cbSize = 22;
            union
            {
                uint16_t wValidBitsPerSample;
                uint16_t wSamplesPerBlock;
                uint16_t wReserved = 0;
            };
            uint32_t dwChannelMask = 0;
            WavGuid SubFormat;
        };
        struct WaveFormat
        {
            uint16_t wFormatTag;
            uint16_t nChannels;
            uint32_t nSamplesPerSec;
            uint32_t nAvgBytesPerSec;
            uint16_t nBlockAlign;
            uint16_t wBitsPerSample;
        };

        // GUIDs in Microsoft order. (little-endian,little-endian,little-endian,big-endian!,bytes)
        static WavGuid WAVE_FORMAT_PCM("00000001-0000-0010-8000-00aa00389b71");
        static WavGuid WAVE_FORMAT_IEEE_FLOAT("00000003-0000-0010-8000-00aa00389b71");

    } // namespace private_use

    enum class ChannelMask
    {
        ZERO = 0x0,
        SPEAKER_FRONT_LEFT = 0x1,
        SPEAKER_FRONT_RIGHT = 0x2,
        SPEAKER_FRONT_CENTER = 0x4,
        SPEAKER_LOW_FREQUENCY = 0x8,
        SPEAKER_BACK_LEFT = 0x10,
        SPEAKER_BACK_RIGHT = 0x20,
        SPEAKER_FRONT_LEFT_OF_CENTER = 0x40,
        SPEAKER_FRONT_RIGHT_OF_CENTER = 0x80,
        SPEAKER_BACK_CENTER = 0x100,
        SPEAKER_SIDE_LEFT = 0x200,
        SPEAKER_SIDE_RIGHT = 0x400,
        SPEAKER_TOP_CENTER = 0x800,
        SPEAKER_TOP_FRONT_LEFT = 0x1000,
        SPEAKER_TOP_FRONT_CENTER = 0x2000,
        SPEAKER_TOP_FRONT_RIGHT = 0x4000,
        SPEAKER_TOP_BACK_LEFT = 0x8000,
        SPEAKER_TOP_BACK_CENTER = 0x10000,
        SPEAKER_TOP_BACK_RIGHT = 0x20000,
    };

    inline ChannelMask operator|(ChannelMask a, ChannelMask b)
    {
        return static_cast<ChannelMask>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline bool operator&(ChannelMask a, ChannelMask b)
    {
        return (static_cast<int>(a) & static_cast<int>(b)) != 0;
    }


    // return's the mask of the n-th channel in the ChannelMask.
    ChannelMask GetChannel(size_t channel,ChannelMask channelMask);

} // namespace toob
