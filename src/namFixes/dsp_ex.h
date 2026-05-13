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

        float GetModelWeight();
        bool IsA2Model();
        bool HasSlimmableSizes();
        const std::vector<float>&  GetSlimmableSizes() const;
        void SetSlimmableSize(double value);

    private:
        void InitNamCoreModel(
            std::unique_ptr<nam::DSP> &&model, 
            const nam::dspData &dspData, 
            float modelSize,
            double sampleRate, 
            size_t maxBlockSize);


        void LoadNamCoreMetadata(const nam::dspData &dspData);
        void LoadNamCoreMetadata(nlohmann::json& jsonModel);
        std::unique_ptr<::NeuralAudio::NeuralModel> neuralAudioModel;
        std::unique_ptr<nam::DSP> namDsp;

        std::vector<float> namInputBuffer;
        std::vector<std::vector<float>> namExtraInputBuffers;
        std::vector<float> namOutputBuffer;
        std::vector<std::vector<float>> namExtraOutputBuffers;
        std::vector<float *> namInputBufferPointers;
        std::vector<float *> namOutputBuffersPointers;
        std::vector<float> slimmableSizes;

        float modelWeight = -1;
        bool isA2Model = false;
        bool hasModelGainDb = false;
        float modelGainDb = 0;
        bool hasModelLoundessDB = false;
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
