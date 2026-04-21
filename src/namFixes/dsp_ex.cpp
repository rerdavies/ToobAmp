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
#include <mutex>

#include "dsp_ex.h"
#include "json.hpp"
#include "NeuralAmpModelerCore/NAM/get_dsp.h"
// #include "NAM/lstm.h"
// #include "NAM/convnet.h"
// #include "NAM/wavenet.h"
// #include "wavenet_t.h"
#include <stdexcept>
#include <iostream>
#include <stdexcept>

bool DEBUG_FORCE_NAM_MODEL = true;

namespace toob
{
    namespace
    {
        static std::mutex ndspMutex;

    };
    std::unique_ptr<NeuralAudioDsp> get_dsp_ex(
        const std::filesystem::path config_filename,
        uint32_t sampleRate,
        int minBlockSize,
        int maxBlockSize)
    {
        {
            std::lock_guard lock{ndspMutex};
            NeuralAudio::NeuralModel::SetDefaultMaxAudioBufferSize(maxBlockSize);
        }

        if (!DEBUG_FORCE_NAM_MODEL)
        {
            ::NeuralAudio::NeuralModel *neuralAudioModel = NeuralAudio::NeuralModel::CreateFromFile(config_filename);
            if (neuralAudioModel)
            {
                neuralAudioModel->SetAudioInputLevelDBu(0); // use our own normalization adjustments.
                return std::make_unique<NeuralAudioDsp>(neuralAudioModel);
            }
        }

        std::unique_ptr<nam::DSP> namDsp = nam::get_dsp(config_filename);
        if (namDsp)
        {
            return std::make_unique<NeuralAudioDsp>(std::move(namDsp),(size_t)maxBlockSize);
        }

        throw std::runtime_error("Invalid file format, or unsupported version.");
    }

    void NeuralAudioDsp::Process(const float *input, float *output, size_t numSamples)
    {
        if (neuralAudioModel)
        {
            neuralAudioModel->Process(const_cast<float*>(input),output,numSamples);
        } else if (namDsp)
        {

            for (size_t i = 0; i < numSamples; ++i)
            {
                namInputBuffer[i] = input[i];
            }
            namDsp->process(namInputBufferPointers.data(), namOutputBuffersPointers.data(), (int)numSamples);

            for (size_t i = 0; i < numSamples; ++i)
            {
                output[i] = namOutputBuffer[i];
            }
        } else {
            for (size_t i = 0; i < numSamples; ++i)
            {
                output[i] = 0;
            }
        }
    }

}
