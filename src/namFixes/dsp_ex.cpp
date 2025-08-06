#if __INTELLISENSE__
#undef __ARM_NEON
#undef __ARM_NEON__
#undef __AVX__
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <memory>

#include "dsp_ex.h"
#include "json.hpp"
// #include "NAM/lstm.h"
// #include "NAM/convnet.h"
// #include "NAM/wavenet.h"
// #include "wavenet_t.h"
#include <stdexcept>
#include <iostream>
#include <stdexcept>


namespace toob
{
    namespace {
        static std::mutex ndspMutex;

    };
    std::unique_ptr<ToobNamDsp> get_dsp_ex(
        const std::filesystem::path config_filename,
        uint32_t sampleRate,
        int minBlockSize,
        int maxBlockSize)
    {
        {
            std::lock_guard lock {ndspMutex};
            NeuralAudio::NeuralModel::SetDefaultMaxAudioBufferSize(maxBlockSize);   
        }


        ToobNamDsp *model = NeuralAudio::NeuralModel::CreateFromFile(config_filename);
        if (model) 
        {
            model->SetAudioInputLevelDBu(0);
        }

        return std::unique_ptr<ToobNamDsp>(model);

    }

}

