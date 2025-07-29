#pragma once


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


#include "NAM/dsp.h"

#include <memory>

namespace nam {
    std::unique_ptr<nam::DSP> get_dsp_ex(
        const std::filesystem::path config_filename, 
        uint32_t sampleRate,
        int minBlockSize, 
        int maxBlockSize);

    int GetPrewarmSamples(nam::DSP *dsp, double sampleRate);
};



#pragma GCC diagnostic pop
