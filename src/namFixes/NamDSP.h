/*
MIT License

Copyright (c) 2022 Steven Atkinson, 2023 Robin Davies

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
#pragma once

  
#include "dsp/dsp.h"

namespace dsp {

#ifdef NAM_SAMPLE_FLOAT
  using nam_float_t = float;
#else 
  using nam_float_t = double;
#endif


// Version of the DSP class that uses buffers of type NAM_FLOAT.
class NamDSP
{
public:

  NamDSP();
  ~NamDSP();
  // The main interface for processing audio.
  // The incoming audio is given as a raw pointer-to-pointers.
  // The indexing is [channel][frame].
  // The output shall be a pointer-to-pointers of matching size.
  // This object instance will own the data referenced by the pointers and be
  // responsible for its allocation and deallocation.
  virtual nam_float_t** Process(nam_float_t** inputs, const size_t numChannels, const size_t numFrames) = 0;

  // Pre-allocate internal buffers.
  // maxFrames specifies the maximum expected frame size.
  void PrepareBuffers(const size_t numChannels, const size_t maxFrames);
  
protected:
  // Methods
  size_t _GetNumChannels() const { return this->mOutputs.size(); };
  size_t _GetNumFrames() const { return this->_GetNumChannels() > 0 ? this->mOutputs[0].size() : 0; }
  // Return a pointer-to-pointers for the DSP's output buffers (all channels)
  // Assumes that ._PrepareBuffers()  was called recently enough.
  nam_float_t** _GetPointers() { return &(mOutputPointers[0]);}
  // Resize mOutputs to (numChannels, numFrames) and ensure that the raw
  // pointers are also keeping up.
  virtual void _PrepareBuffers(const size_t numChannels, const size_t numFrames);

  // The output array into which the DSP module's calculations will be written.
  // Pointers to this member's data will be returned by .Process(), and std
  // Will ensure proper allocation.
  std::vector<std::vector<nam_float_t>> mOutputs;
  std::vector<nam_float_t*> mOutputPointers;
};
  
} // namespace dsp
