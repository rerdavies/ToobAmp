/*
MIT License

Copyright (c) 2025 Robin E. R. Davies

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
#include <cstdint>
#include <cstddef>

namespace toob::nam_impl {

    /* Patch Get property that returns the current NAM Model's medata.
        Returns a vector of floats.
    */

    #define TOOB_NAM__MODEL_METADATA "http://two-play.com/plugins/toob-nam#model_metadata"

    // offset 0: an integer with the folloing bits set.
    enum TOOB_NAM_METADATA_OFFSETS {
        flags = 0,
        preset_version,
        loudness,
        gain,
        input_level_dbu,
        output_level_dbu,
        max_metadata_offset 
    };


    enum TOOB_NAM_METADATA_FLAGS {
        has_model = 1,
        has_loudness = 2,
        has_gain = 4,
        has_input_level_dbu = 8,
        has_output_level_dbu = 16    
    };



};
