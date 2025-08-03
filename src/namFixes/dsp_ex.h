#pragma once


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


#include <memory>

#include "NeuralAudio/NeuralModel.h"



namespace toob {

    using ToobNamDsp = ::NeuralAudio::NeuralModel;

    std::unique_ptr<ToobNamDsp> get_dsp_ex(
        const std::filesystem::path config_filename, 
        uint32_t sampleRate,
        int minBlockSize, 
        int maxBlockSize);

};



#pragma GCC diagnostic pop
