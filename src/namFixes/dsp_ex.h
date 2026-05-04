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
        NeuralAudioDsp(::NeuralAudio::NeuralModel *model);
        NeuralAudioDsp(std::unique_ptr<nam::DSP> &&model, const nam::dspData &dspData,double sampleRate,size_t maxBlockSize, const std::vector<double>&slimmableSizes);

        void Process(const float *input, float *output, size_t numSamples);

        bool HasModelGainDB();
        float GetModelGainDB();
        bool HasModelLoudnessDB();
        float GetModelLoudnessDB();
        bool HasModelInputLevelDBu();
        float GetModelInputLevelDBu();
        bool HasModelOutputLevelDBu();
        float GetModelOutputLevelDBu();

        bool HasSlimmableSizes();
        const std::vector<double>&  GetSlimmableSizes() const;
        void SetSlimmableSize(double value);

    private:
        void LoadNamCoreMetadata(const nam::dspData &dspData);
        std::unique_ptr<::NeuralAudio::NeuralModel> neuralAudioModel;
        std::unique_ptr<nam::DSP> namDsp;

        std::vector<float> namInputBuffer;
        std::vector<std::vector<float>> namExtraInputBuffers;
        std::vector<float> namOutputBuffer;
        std::vector<std::vector<float>> namExtraOutputBuffers;
        std::vector<float *> namInputBufferPointers;
        std::vector<float *> namOutputBuffersPointers;
        std::vector<double> slimmableSizes;

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
        uint32_t sampleRate,
        int minBlockSize,
        int maxBlockSize);

};

#pragma GCC diagnostic pop
