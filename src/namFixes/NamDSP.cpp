/*
MIT License

Copyright (c) 2022 Steven Atkinson, 2023 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "NamDSP.h"

#include <algorithm> // std::max_element
#include <algorithm>
#include <cmath> // pow, tanh, expf
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>


// ============================================================================
// Implementation of Version 2 interface

dsp::NamDSP::NamDSP()
{
}

dsp::NamDSP::~NamDSP()
{
};
void dsp::NamDSP::PrepareBuffers(const size_t numChannels, const size_t maxFrameSize)
{
    // subsequent resizes use the allocated backing buffers, as long as numFrames < maxFrameSize.
    _PrepareBuffers(numChannels,maxFrameSize);
}
void dsp::NamDSP::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const size_t oldFrames = this->_GetNumFrames();
  const size_t oldChannels = this->_GetNumChannels();

  const bool resizeChannels = oldChannels != numChannels;
  const bool resizeFrames = resizeChannels || (oldFrames != numFrames);

  if (resizeFrames || resizeChannels)
  {
    if (resizeChannels)
    {
        this->mOutputs.resize(numChannels);
        this->mOutputPointers.resize(resizeChannels);
    }
    for (size_t c = 0; c < numChannels; c++)
    {
      this->mOutputs[c].resize(numFrames);
      this->mOutputPointers[c] = &(this->mOutputs[c][0]);
    }
  }
}
