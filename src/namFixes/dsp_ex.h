#pragma once


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


#include "NAM/dsp.h"

namespace nam {
    std::unique_ptr<nam::DSP> get_dsp_ex(const std::filesystem::path config_filename, int minBlockSize, int maxBlockSize);
};

#pragma GCC diagnostic pop
