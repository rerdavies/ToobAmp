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

#include "Mp3Reader.hpp"
#include <stdexcept>
#include <fstream>
#include <cstdint>
#include <cstring>
#include "ss.hpp"

#include "minimp3/minimp3_ex.h"

using namespace toob;

AudioData Mp3Reader::Load(const std::filesystem::path &path)
{
    mp3dec_t dec;
    mp3dec_file_info_t info;
    info.buffer = nullptr;

    int rc = mp3dec_load(&dec, path.c_str(), &info, nullptr, nullptr);
    if (rc != 0)
    {
        throw std::runtime_error(SS("Failed to load MP3 file: " << path));
    }
    if (!info.buffer || info.samples == 0)
    {
        free(info.buffer);
        throw std::runtime_error(SS("Empty or invalid MP3 file: " << path));
    }

    size_t frames = info.samples / info.channels;
    AudioData result(info.hz, info.channels, frames);

    // Deinterleave int16_t samples into per-channel float buffers
    static constexpr float SCALE = 1.0f / 32768.0f;
    for (int c = 0; c < info.channels; ++c)
    {
        std::vector<float> &channel = result.getChannel(c);
        for (size_t i = 0; i < frames; ++i)
        {
            channel[i] = info.buffer[i * info.channels + c] * SCALE;
        }
    }

    free(info.buffer);
    return result;
}
bool Mp3Reader::IsMp3File(const std::filesystem::path &path)
{
    if (path.extension() != ".mp3")
    {
        return false;
    }
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    uint8_t header[3];
    f.read(reinterpret_cast<char *>(header), 3);
    if (f.gcount() < 3)
        return false;
    // ID3v2 tag header
    if (header[0] == 0x49 && header[1] == 0x44 && header[2] == 0x33) // "ID3"
        return true;
    return false;
}
