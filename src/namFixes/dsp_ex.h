#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#ifndef NAM_SAMPLE_FLOAT
#define NAM_SAMPLE_FLOAT 1
#endif

#include <memory>

#include "NeuralAudio/NeuralModel.h"
#include "NeuralAmpModelerCore/Dependencies/nlohmann/json.hpp"
#include "NeuralAmpModelerCore/NAM/dsp.h"

namespace toob
{


    class NeuralAudioDsp
    {
    public:
        NeuralAudioDsp(::NeuralAudio::NeuralModel *model)
        {
            neuralAudioModel = std::unique_ptr<::NeuralAudio::NeuralModel>(model);
        }
        NeuralAudioDsp(std::unique_ptr<nam::DSP> &&model, size_t maxBlockSize)
        {
            namDsp = std::move(model);

            namInputBuffer.resize(maxBlockSize);
            namOutputBuffer.resize(maxBlockSize);

            namInputBufferPointers.resize(namDsp->NumInputChannels() + 1); // +1 to include an extra nullptr as a guard.
            namInputBufferPointers[0] = namInputBuffer.data();
            if (namDsp->NumInputChannels() > 1)
            {
                namExtraInputBuffers.resize(namDsp->NumInputChannels() - 1);
                for (int i = 1; i < namDsp->NumInputChannels(); ++i)
                {
                    namExtraInputBuffers[i - 1].resize(maxBlockSize);
                    namInputBufferPointers[i] = namExtraInputBuffers[i - 1].data();
                }
            }

            namOutputBuffersPointers.resize(namDsp->NumOutputChannels() + 1); // +1 to include an extra nullptr as a guard.
            namOutputBuffersPointers[0] = namOutputBuffer.data();
            if (namDsp->NumOutputChannels() > 1)
            {
                namExtraInputBuffers.resize(namDsp->NumOutputChannels()-1);
                for (int i = 1; i < namDsp->NumOutputChannels(); ++i)
                {
                    namExtraOutputBuffers[i-1].resize(maxBlockSize);
                    namInputBufferPointers[i] = namExtraOutputBuffers[i-1].data();
                }
            }
            namDsp->prewarm();
        }

        void Process(const float *input, float *output, size_t numSamples);

        bool HasModelGainDB()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->HasModelGainDB();
            }
            return false;
        }
        float GetModelGainDB()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->GetModelGainDB();
            }
            return 0;
        }
        bool HasModelLoudnessDB()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->HasModelLoudnessDB();
            }
            if (namDsp)
            {
                return namDsp->HasLoudness();
            }
            return false;
        }
        float GetModelLoudnessDB()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->GetModelLoudnessDB();
            }
            if (namDsp)
            {
                return namDsp->GetLoudness();
            }
            return 0;
        }
        bool HasModelInputLevelDBu()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->HasModelInputLevelDBu();
            }
            if (namDsp)
            {
                return namDsp->GetInputLevel();
            }
            return false;
        }
        float GetModelInputLevelDBu()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->GetModelInputLevelDBu();
            }
            if (namDsp)
            {
                return namDsp->GetInputLevel();
            }
            return 0;
        }
        bool HasModelOutputLevelDBu()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->HasModelOutputLevelDBu();
            }
            if (namDsp)
            {
                return namDsp->HasOutputLevel();
            }
            return false;
        }
        float GetModelOutputLevelDBu()
        {
            if (neuralAudioModel)
            {
                return neuralAudioModel->GetModelOutputLevelDBu();
            }
            if (namDsp)
            {
                return namDsp->GetOutputLevel();
            }
            return 0;
        }

    private:
        std::unique_ptr<::NeuralAudio::NeuralModel> neuralAudioModel;
        std::unique_ptr<nam::DSP> namDsp;

        std::vector<float> namInputBuffer;
        std::vector<std::vector<float>> namExtraInputBuffers;
        std::vector<float> namOutputBuffer;
        std::vector<std::vector<float>> namExtraOutputBuffers;
        std::vector<float *> namInputBufferPointers;
        std::vector<float *> namOutputBuffersPointers;
    };

    std::unique_ptr<NeuralAudioDsp> get_dsp_ex(
        const std::filesystem::path config_filename,
        uint32_t sampleRate,
        int minBlockSize,
        int maxBlockSize);

};

#pragma GCC diagnostic pop
