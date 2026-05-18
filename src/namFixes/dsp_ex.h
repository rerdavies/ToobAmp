#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#ifndef NAM_SAMPLE_FLOAT
#define NAM_SAMPLE_FLOAT 1
#endif

#include <memory>

#include "NeuralAudio/NeuralModel.h"

namespace toob
{


    enum class NamModelType 
    {
        None = 0, 
        A1 = 1,
        A2 = 2,
        AidaX = 3,
    };
    class NeuralAudioDsp
    {
    public:
        NeuralAudioDsp(
            const std::filesystem::path&path, 
            float modelWeight,
            uint32_t sampleRate,
            int minBlockSize,
            int maxBlockSize);

        void Process(const float *input, float *output, size_t numSamples);

        bool HasModelGainDB();
        float GetModelGainDB();
        bool HasModelLoudnessDB();
        float GetModelLoudnessDB();
        bool HasModelInputLevelDBu();
        float GetModelInputLevelDBu();
        bool HasModelOutputLevelDBu();
        float GetModelOutputLevelDBu();
        NamModelType GetModelType();
        float GetModelWeight();
        bool HasSlimmableSizes();
        void SetSlimmableSize(double value);
        void Prewarm();

    private:
        void loadMetadataProperty(const char*name, bool &hasValue, float &value);
        void loadNeuralAudioMetadata();

        std::unique_ptr<::NeuralAudio::NeuralModel> neuralAudioModel;


        NamModelType modelType = NamModelType::None;
        float modelWeight = -1;
        bool hasSlimmableSizes = false;
        bool hasModelGainDb = false;
        float modelGainDb = 0;
        bool hasModelLoudnessDB = false;
        float modelLoudnessDB = 0;
        bool hasModelInputLevelDBu = false;
        float modelInputLevelDBu = 0;
        bool hasModelOutputLevelDBu = false;
        float modelOutputLevelDBu = 0;

    };

    std::unique_ptr<NeuralAudioDsp> get_dsp_ex(
        const std::filesystem::path config_filename,
        float modelSize,
        uint32_t sampleRate,
        int minBlockSize,
        int maxBlockSize);

};

#pragma GCC diagnostic pop
