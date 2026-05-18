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
        return hasModelLoudnessDB;
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

    float NeuralAudioDsp::GetModelWeight()
    {
        return this->modelWeight;
    }
    NamModelType NeuralAudioDsp::GetModelType()
    {
        return this->modelType;
    }



    float NeuralAudioDsp::GetModelOutputLevelDBu()
    {
        return modelOutputLevelDBu;
    }

    std::unique_ptr<NeuralAudioDsp> get_dsp_ex(
        const std::filesystem::path config_filename,
        float modelWeight,
        uint32_t sampleRate,
        int minBlockSize,
        int maxBlockSize)
    {
        return std::make_unique<NeuralAudioDsp>(
            config_filename,
            modelWeight,
            sampleRate,
            minBlockSize,
            maxBlockSize

        );
    }

    NeuralAudioDsp::NeuralAudioDsp(
        const std::filesystem::path &config_filename,
        float modelWeight,
        uint32_t sampleRate,
        int minBlockSize,
        int maxBlockSize)
    {

        {
            std::lock_guard lock{ndspMutex};
            NeuralAudio::NeuralModel::SetDefaultMaxAudioBufferSize(maxBlockSize);
        }

        ::NeuralAudio::NeuralModel *neuralAudioModel = NeuralAudio::NeuralModel::CreateFromFile(config_filename);
        if (neuralAudioModel)
        {


            this->neuralAudioModel = std::unique_ptr<::NeuralAudio::NeuralModel>(neuralAudioModel);
            neuralAudioModel->SetAudioInputLevelDBu(0); // use our own normalization adjustments.
            if (neuralAudioModel->HasQualityScaling())
            {
                neuralAudioModel->SetQualityScaleFactor(modelWeight);
            }
            neuralAudioModel->Prewarm();

            loadNeuralAudioMetadata();

            #ifdef TOOB_OPTIMIZATION_FLAGS
            std::cout << TOOB_OPTIMIZATION_FLAGS << " ";
            #endif
            std::cout << 
                "Load mode: " << (neuralAudioModel->GetLoadMode() == NeuralAudio::EModelLoadMode::NAMCore ? "NAM Core" : "NeuralAudio");
            if (neuralAudioModel->HasQualityScaling())
            {
                std::cout << " A2 weight=" << modelWeight;;
            }
            std::cout << std::endl;

            return;
        }
        throw std::runtime_error("Invalid file format, or unsupported version.");
    }

    void NeuralAudioDsp::Process(const float *input, float *output, size_t numSamples)
    {
        if (neuralAudioModel)
        {
            neuralAudioModel->Process(const_cast<float *>(input), output, numSamples);
        }
        else
        {
            for (size_t i = 0; i < numSamples; ++i)
            {
                output[i] = 0;
            }
        }
    }

    void NeuralAudioDsp::Prewarm() 
    {
        if (neuralAudioModel)
        {
            neuralAudioModel->Prewarm();
        }
    }
    void NeuralAudioDsp::SetSlimmableSize(double value)
    {
        if (neuralAudioModel)
        {
            neuralAudioModel->SetQualityScaleFactor(value);
        }
    }

    void NeuralAudioDsp::loadMetadataProperty(const char*name, bool &hasValue, float &value)
    {
        double result;
        if (neuralAudioModel->GetMetadata(name,result))
        {
            hasValue = true;
            value = (float)result;
        } else {
            hasValue = false;
        }
    }

    void NeuralAudioDsp::loadNeuralAudioMetadata()
    {
        if (neuralAudioModel)
        {
            loadMetadataProperty("gain",hasModelGainDb, modelGainDb);
            loadMetadataProperty("loudness",hasModelLoudnessDB, modelLoudnessDB);
            loadMetadataProperty("input_level_dbu",hasModelInputLevelDBu, modelInputLevelDBu);
            loadMetadataProperty("output_level_dbu",hasModelOutputLevelDBu, modelOutputLevelDBu);

            if (neuralAudioModel->GetLoadMode() == NeuralAudio::EModelLoadMode::RTNeural)
            {
                this->modelType = NamModelType::AidaX;
                double value;
                if (neuralAudioModel->GetMetadata("in_gain",value))
                {
                    hasModelInputLevelDBu = true;
                    modelInputLevelDBu = (float)value;
                }
                if (neuralAudioModel->GetMetadata("out_gain",value))
                {
                    hasModelLoudnessDB = true;
                    modelLoudnessDB = -18-(float)value;
                }
            } else {
                switch (neuralAudioModel->GetLoadMode()) {
                    case NeuralAudio::EModelLoadMode::Internal:
                        this->modelType = NamModelType::A1;
                        break;
                    case NeuralAudio::EModelLoadMode::NAMCore:
                        this->modelType = NamModelType::A2;
                        break;
                    default:
                        throw std::runtime_error("Unknown NeuralAudio model type");
                }
                this->modelType = neuralAudioModel->GetLoadMode() == NeuralAudio::EModelLoadMode::NAMCore ? NamModelType::A2: NamModelType::A1;
                this->modelWeight = neuralAudioModel->GetQualityScaleFactor();
                loadMetadataProperty("gain",hasModelGainDb, modelGainDb);
                loadMetadataProperty("loudness",hasModelLoudnessDB, modelLoudnessDB);
                loadMetadataProperty("input_level_dbu",hasModelInputLevelDBu, modelInputLevelDBu);
                loadMetadataProperty("output_level_dbu",hasModelOutputLevelDBu, modelOutputLevelDBu);
            }
            hasSlimmableSizes = neuralAudioModel->HasQualityScaling();


        }

    }

    bool NeuralAudioDsp::HasSlimmableSizes()
    {
        return hasSlimmableSizes;
    }

}
