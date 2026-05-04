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
#include "NeuralAmpModelerCore/NAM/container.h"
// #include "NAM/lstm.h"
// #include "NAM/convnet.h"
// #include "NAM/wavenet.h"
// #include "wavenet_t.h"
#include <stdexcept>
#include <iostream>
#include <stdexcept>

bool DEBUG_FORCE_NAM_MODEL = false;

namespace toob
{
    namespace
    {
        static std::mutex ndspMutex;

    };

    NeuralAudioDsp::NeuralAudioDsp(::NeuralAudio::NeuralModel *model)
    {
        neuralAudioModel = std::unique_ptr<::NeuralAudio::NeuralModel>(model);
        if (model->HasModelGainDB())
        {
            this->hasModelGainDb = true;
            this->modelGainDb = model->GetModelGainDB();
        }
        if (model->HasModelLoudnessDB())
        {
            this->hasModelLoundessDB = true;
            this->modelLoudnessDB = model->GetModelLoudnessDB();
        }
        if (model->HasModelInputLevelDBu())
        {
            this->hasModelInputLevelDBu = true;
            this->modelInputLevelDBu = model->GetModelInputLevelDBu();
        }
        if (model->HasModelOutputLevelDBu())
        {
            this->hasModelOutputLevelDBu = true;
            this->modelOutputLevelDBu = model->GetModelOutputLevelDBu();
        }
    }

    NeuralAudioDsp::NeuralAudioDsp(std::unique_ptr<nam::DSP> &&model, const nam::dspData &dspData, double sampleRate, size_t maxBlockSize, const std::vector<double>&slimmableSizes)
    : slimmableSizes(slimmableSizes)
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
            namExtraInputBuffers.resize(namDsp->NumOutputChannels() - 1);
            for (int i = 1; i < namDsp->NumOutputChannels(); ++i)
            {
                namExtraOutputBuffers[i - 1].resize(maxBlockSize);
                namInputBufferPointers[i] = namExtraOutputBuffers[i - 1].data();
            }
        }

        LoadNamCoreMetadata(dspData);

        namDsp->ResetAndPrewarm(sampleRate, (int)maxBlockSize);
    }

    void NeuralAudioDsp::LoadNamCoreMetadata(const nam::dspData &dspData)
    {
        if (namDsp->HasLoudness())
        {
            this->hasModelLoundessDB = true;
            this->modelLoudnessDB = namDsp->GetLoudness();
        }
        if (namDsp->HasInputLevel())
        {
            this->hasModelInputLevelDBu = true;
            this->modelInputLevelDBu = namDsp->GetInputLevel();
        }
        if (namDsp->HasOutputLevel())
        {
            this->hasModelOutputLevelDBu = true;
            this->modelOutputLevelDBu = namDsp->GetOutputLevel();
        }
    }

    bool NeuralAudioDsp::HasModelGainDB()
    {
        return hasModelGainDb;
    }

    float NeuralAudioDsp::GetModelGainDB()
    {
        return modelGainDb;
    }

    bool NeuralAudioDsp::HasModelLoudnessDB()
    {
        return hasModelLoundessDB;
    }

    float NeuralAudioDsp::GetModelLoudnessDB()
    {
        return modelLoudnessDB;
    }

    bool NeuralAudioDsp::HasModelInputLevelDBu()
    {
        return hasModelInputLevelDBu;
    }

    float NeuralAudioDsp::GetModelInputLevelDBu()
    {
        return modelInputLevelDBu;
    }

    bool NeuralAudioDsp::HasModelOutputLevelDBu()
    {
        return hasModelOutputLevelDBu;
    }

    float NeuralAudioDsp::GetModelOutputLevelDBu()
    {
        return modelOutputLevelDBu;
    }

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
#ifdef A76_OPTIMIZATION
                std::cout << "Using NeuralAudio backend (A76)" << std::endl;
#else
                std::cout << "Using NeuralAudio backend." << std::endl;
#endif
                neuralAudioModel->SetAudioInputLevelDBu(0); // use our own normalization adjustments.
                return std::make_unique<NeuralAudioDsp>(neuralAudioModel);
            }
        }

        if (!std::filesystem::exists(config_filename))
            throw std::runtime_error("Config file doesn't exist!\n");
        std::ifstream i(config_filename);
        nlohmann::json j;
        i >> j;


        
        // If using submodules, select the submodule.
        nlohmann::json *actualModel = &j;

        constexpr double modelWeight = 0.5;



        std::string architecture = j["architecture"].get<std::string>();

        std::vector<double> slimmableSizes;

        if (architecture == "SlimmableContainer")
        {
            auto &config = j["config"];

            auto &submodels_json = config["submodels"];
            if (submodels_json.is_array())
            {
                for (auto &entry : submodels_json)
                {
                    double max_val = entry.at("max_value").get<double>();
                    slimmableSizes.push_back(max_val);
                    if (max_val <= modelWeight)
                    {
                        actualModel = &entry.at("model");
                    }
                }
            }
        } 
        nam::dspData dspData;
        std::unique_ptr<nam::DSP> namDsp = nam::get_dsp(*actualModel, dspData);
        if (namDsp)
        {
#ifdef A76_OPTIMIZATION
            std::cout << "Using NAM Core backend (A76)" << std::endl;
#else
            std::cout << "Using NAM Core backend." << std::endl;
#endif

            return std::make_unique<NeuralAudioDsp>(std::move(namDsp), dspData, (double)sampleRate, (size_t)maxBlockSize, slimmableSizes);
        }

        throw std::runtime_error("Invalid file format, or unsupported version.");
    }

    void NeuralAudioDsp::Process(const float *input, float *output, size_t numSamples)
    {
        if (neuralAudioModel)
        {
            neuralAudioModel->Process(const_cast<float *>(input), output, numSamples);
        }
        else if (namDsp)
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
        }
        else
        {
            for (size_t i = 0; i < numSamples; ++i)
            {
                output[i] = 0;
            }
        }
    }

    bool NeuralAudioDsp::HasSlimmableSizes()
    {
        if (namDsp)
        {
            nam::container::ContainerModel *pContainer = dynamic_cast<nam::container::ContainerModel *>(namDsp.get());
            return pContainer != nullptr;
        }
        return false;
    }
    const std::vector<double> &NeuralAudioDsp::GetSlimmableSizes() const
    {
        return slimmableSizes;
    }
    void NeuralAudioDsp::SetSlimmableSize(double value)
    {
        if (namDsp)
        {
            nam::container::ContainerModel *pContainer = dynamic_cast<nam::container::ContainerModel *>(namDsp.get());
            if (pContainer)
            {
                pContainer->SetSlimmableSize(value);
            }
        }
    }

}
