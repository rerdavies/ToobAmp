/*
MIT License

Copyright (c) 2022 Steven Atkinson, 2023 Robin Davies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/**************************
    From https://github.com/sdatkinson/NeuralAmpModelerPlugin/

    Ported to LV2 by Robin Davies

*************/
#pragma once

// Reduce warnings for NueralAmpModelerCore files.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "NeuralAmpModelerCore/NAM/dsp.h"
#include "NeuralAmpModelerCore/dsp/ImpulseResponse.h"
#include "namFixes/NoiseGate.h"
#include "NeuralAmpModelerCore/dsp/RecursiveLinearFilter.h"
#include "NeuralAmpModelerCore/dsp/dsp.h"
#include "NeuralAmpModelerCore/dsp/wav.h"
#include "LsNumerics/BaxandallToneStack.hpp"
#include "LsNumerics/ToneStackFilter.h"

#include "FilterResponse.h"


#pragma GCC diagnostic pop

#include "Lv2Plugin.h"
#include <atomic>
#include "InputPort.h"
#include "OutputPort.h"
#include <cstddef>

namespace toob
{

    class NeuralAmpModeler final : public Lv2PluginWithState
    {
    public:
        static const char URI[];

        using float_t = float;   // external float type.
#ifdef NAM_SAMPLE_FLOAT
        using nam_float_t = float; // internal float type.
#else
        using nam_float_t = double; // internal float type.
#endif

        using IR = dsp::ImpulseResponse;

        static Lv2Plugin *Create(double rate,
                                 const char *bundle_path,
                                 const LV2_Feature *const *features)
        {
            return new NeuralAmpModeler(rate, bundle_path, features);
        }
        NeuralAmpModeler(double rate,
                         const char *bundle_path,
                         const LV2_Feature *const *features);

        ~NeuralAmpModeler();

        enum class EParams
        {
            kInputLevel = 0,
            kOutputLevel,
            kNoiseGateThreshold,
            kGateOut,
            kBass,
            kMid,
            kTreble,
            kStackType,

            kAudioIn,
            kAudioOut,
            kControlIn,
            kControlOut
        };
        bool LoadModel(const std::string&filename); // (for tests)
    private:
        struct Urids
        {
            void Initialize(NeuralAmpModeler &this_);
            uint32_t nam__ModelFileName;
            uint32_t nam__FrequencyResponse;
            uint32_t atom__Path;
            uint32_t atom__String;

			LV2_URID patch;
			LV2_URID patch__Get;
			LV2_URID patch__Set;
			LV2_URID patch__property;
			LV2_URID patch__value;
			LV2_URID atom__URID;
			LV2_URID atom__float;
			LV2_URID atom__int;
			LV2_URID units__Frame;


        };


        Urids urids;
        void ConnectPort(uint32_t port, void *data) override;
        void Activate() override;
        void Run(uint32_t n_samples) override;
        void Deactivate() override;
        LV2_State_Status
        OnRestoreLv2State(
            LV2_State_Retrieve_Function retrieve,
            LV2_State_Handle handle,
            uint32_t flags,
            const LV2_Feature *const *features) override;

        virtual LV2_State_Status
        OnSaveLv2State(
            LV2_State_Store_Function store,
            LV2_State_Handle handle,
            uint32_t flags,
            const LV2_Feature *const *features) override;

        void OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *value) override;
        void OnPatchGet(LV2_URID propertyUrid) override;

        void ProcessBlock(int nFrames);
        void OnReset();
        void OnIdle();

    private:
        std::string UnmapFilename(const LV2_Feature *const *features, const std::string &fileName);

        std::string MapFilename(
            const LV2_Feature *const *features,
            const std::string &input,
            const char *browserPath);

        virtual LV2_Worker_Status OnWork(
            LV2_Worker_Respond_Function respond,
            LV2_Worker_Respond_Handle handle,
            uint32_t size,
            const void *data) override;

        virtual LV2_Worker_Status OnWorkResponse(uint32_t size, const void *data) override;

    private:
        double rate = 44100;
        std::string bundle_path;

        const int kNumPresets = 1;

        RangedDbInputPort cInputGain{-20, 20};
        RangedDbInputPort cOutputGain{-40, 40};
        RangedDbInputPort cNoiseGateThreshold{-100, 0};
        RangedInputPort cBass {0,10};
        RangedInputPort cMid{ 0,10};
        RangedInputPort cTreble{ 0,10};
        EnumeratedInputPort cToneStackType { 4};

        enum ToneStackType {
            Bassman = 0, // matches enum values in .ttl file.
            Jcm8000 = 1,
            Baxandall = 2,
            Bypass = 3,
        };
        ToneStackType toneStackType = ToneStackType::Bypass;
		ToneStackFilter toneStackFilter;
		BaxandallToneStack baxandallToneStack;


        bool noiseGateActive = false;
        OutputPort cGateOutput;
        const float *audioIn = nullptr;
        float *audioOut = nullptr;
        LV2_Atom_Sequence *controlIn = nullptr;
        LV2_Atom_Sequence *controlOut = nullptr;

        int gateOutputUpdateRate = 100;
        int gateOutputUpdateCount = 0;
        bool isActivated = false;
        bool requestFileUpdate = true;


        FilterResponse filterResponse;

        bool responseGet = false;
        int64_t responseDelaySamplesMax = 0;
        int64_t responseDelaySamples = 0;
        uint64_t responseDelayMsMax = 0;
        uint64_t responseDelayMs = 0;

    private:
        // Update tone stack filter designs.
        void UpdateToneStack();
        // Write frequency response for UI.
        void WriteFrequencyResponse();

        // Frequency response of the tone stack at specified frequency.
        float CalculateFrequencyResponse(float hz);



        // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
        void _FallbackDSP(nam_float_t **inputs, nam_float_t **outputs, const size_t numChannels, const size_t numFrames);
        // Sizes based on mInputArray
        size_t _GetBufferNumChannels() const;
        size_t _GetBufferNumFrames() const;
        // Gets a new Neural Amp Model
        // Throws an exception on error.
        std::unique_ptr<DSP> _GetNAM(const std::string &dspFile);

        bool _HaveModel() const { return this->mNAM != nullptr; };
        // Prepare the input & output buffers
        void _PrepareBuffers(const size_t numFrames);
        // Manage pointers
        void _PrepareIOPointers(const size_t nChans);
        // Copy the input buffer to the object, applying input level.
        // :param nChansIn: In from external
        // :param nChansOut: Out to the internal of the DSP routine
        void _ProcessInput(const float_t **input, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
        // Copy the output to the output buffer, applying output level.
        // :param nChansIn: In from internal
        // :param nChansOut: Out to external
        void _ProcessOutput(nam_float_t **inputs, float_t **outputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);

        // Member data

        // The plugin is mono inside
        const size_t mNUM_INTERNAL_CHANNELS = 1;

        // Input arrays to NAM
        std::vector<std::vector<nam_float_t>> mInputArray;
        // Output from NAM
        std::vector<std::vector<nam_float_t>> mOutputArray;
        // Pointer versions
        std::vector<nam_float_t*> mInputPointerMemory;
        std::vector<nam_float_t*> mOutputPointerMemory;
        nam_float_t **mInputPointers = nullptr;
        nam_float_t **mOutputPointers = nullptr;

        std::vector<nam_float_t> mToneStackArray;
        nam_float_t *mToneStackPointer = nullptr;

        // Noise gates
        dsp::noise_gate::Trigger mNoiseGateTrigger;
        dsp::noise_gate::Gain mNoiseGateGain;

        // The Neural Amp Model (NAM) actually being used:
        std::unique_ptr<DSP> mNAM;

        // Path to model's config.json or model.nam
        std::string mNAMPath;

        std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};
    };

} // namespace