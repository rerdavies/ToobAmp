/*
MIT License

Copyright (c) 2022 Steven Atkinson, 2023-2024 Robin E. R. Davies

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

#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"

#ifdef __CLANG__
#pragma GCC diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

#if __INTELLISENSE__
#undef __ARM_NEON
#undef __ARM_NEON__
#endif

#include <Eigen/Core>
#include <algorithm> // std::clamp
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utility>
#include <vector>
#include "lv2ext/filedialog.h"
#include "ss.hpp"
#include <cfenv>
#include "LsNumerics/Denorms.hpp"
#include <Eigen/Dense>
#include <filesystem>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"

#include "Eigen/Eigen"
#include "NeuralAmpModelerCore/NAM/activations.h"
#include "nam_architecture.hpp"

#include "namFixes/dsp_ex.h"


#pragma GCC diagnostic pop

// clang-format off

#include "NeuralAmpModeler.h"

using namespace toob;
using namespace nam;


#include <time.h>
uint64_t timeMs();


const int INPUT_LEVEL_MIN=-35;
const int INPUT_LEVEL_MAX=10;

// clang-format on
// #include "architecture.hpp"

const char NeuralAmpModeler::URI[] = "http://two-play.com/plugins/toob-nam";

enum class NamMessageType
{
    Load,
    FreeLoad,

    LoadResponse
};

static constexpr size_t MAX_NAM_FILENAME = 1023;

class NamMessage
{
public:
    NamMessage(NamMessageType messageType)
        : messageType(messageType)
    {
    }
    NamMessageType MessageType() const { return messageType; }

private:
    NamMessageType messageType;
};

class NamFreeMessage : public NamMessage
{
public:
    NamFreeMessage(DSP *dsp)
        : NamMessage(NamMessageType::FreeLoad),
          dsp(dsp)
    {
    }

    void Work()
    {
        delete dsp;
    }

private:
    DSP *dsp;
};

class NamLoadMessage : public NamMessage
{
protected:
    NamLoadMessage(NamMessageType messageType, const char *modelFileName)
        : NamMessage(messageType)
    {
        SetFileName(modelFileName);
    }

    void SetFileName(const char *modelFileName)
    {
        hasModel = modelFileName != nullptr;
        memset(this->modelFileName, 0, sizeof(this->modelFileName));
        if (hasModel)
        {
            strcpy(this->modelFileName, modelFileName);
        }
    }

public:
    NamLoadMessage(const char *modelFileName)
        : NamMessage(NamMessageType::Load)
    {
        SetFileName(modelFileName);
    }
    bool HasModel() const { return hasModel; }
    const char *ModelFileName() const
    {
        return hasModel ? modelFileName : nullptr;
    }

private:
    bool hasModel;
    char modelFileName[MAX_NAM_FILENAME + 1];
};

class NamLoadResponseMessage : public NamLoadMessage
{
public:
    NamLoadResponseMessage(
        const char *modelFileName,
        DSP *modelObject)
        : NamLoadMessage(NamMessageType::LoadResponse, modelFileName),
          modelObject(modelObject)
    {
    }
    DSP *modelObject;
};

NeuralAmpModeler::NeuralAmpModeler(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2PluginWithState(bundle_path, features),
      rate(rate),
      mInputPointers(nullptr),
      mOutputPointers(nullptr),
      mNoiseGateTrigger(),
      mNAM(nullptr),
      mNAMPath()
{

    mNAMPath.reserve(MAX_NAM_FILENAME + 1);

    urids.Initialize(*this);

    using namespace ::activations;
    Activation::enable_fast_tanh();
    this->mNoiseGateTrigger.AddListener(&mNoiseGateGain);
    // update gate output 15 times a second.
    this->gateOutputUpdateRate = (int)(rate / 15);

    // frequecy response out latency
    // frequency response variables.
    constexpr double UPDATE_RATE = (1.0 / 15.0); // 15 times a second.
    responseDelaySamplesMax = (int64_t)(UPDATE_RATE * rate);

    this->toneStackFilter.SetSampleRate(rate);
    this->baxandallToneStack.SetSampleRate(rate);

    // INPUT sample rate
    const int32_t INPUT_UPDATES_PER_SEC = 15;
    vuMaxSampleCount = uint32_t(rate / INPUT_UPDATES_PER_SEC);
}
void NeuralAmpModeler::Urids::Initialize(NeuralAmpModeler &this_)
{
    atom__Path = this_.MapURI(LV2_ATOM__Path);
    atom__String = this_.MapURI(LV2_ATOM__String);
    nam__ModelFileName = this_.MapURI("http://two-play.com/plugins/toob-nam#modelFile");
    nam__FrequencyResponse = this_.MapURI("http://two-play.com/plugins/toob-nam#FrequencyResponse");
    patch = this_.MapURI(LV2_PATCH_URI);
    patch__Get = this_.MapURI(LV2_PATCH__Get);
    patch__Set = this_.MapURI(LV2_PATCH__Set);
    patch__property = this_.MapURI(LV2_PATCH__property);
    patch__value = this_.MapURI(LV2_PATCH__value);
    atom__URID = this_.MapURI(LV2_ATOM__URID);
    atom__float = this_.MapURI(LV2_ATOM__Float);
    atom__int = this_.MapURI(LV2_ATOM__Int);
    units__Frame = this_.MapURI(LV2_UNITS__frame);
}

std::string NeuralAmpModeler::UnmapFilename(const LV2_Feature *const *features, const std::string &fileName)
{
    // const LV2_State_Make_Path *makePath = GetFeature<LV2_State_Make_Path>(features, LV2_STATE__makePath);
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath)
    {
        char *result = mapPath->abstract_path(mapPath->handle, fileName.c_str());
        std::string t = result;
        if (freePath)
        {
            freePath->free_path(freePath->handle, result);
        }
        else
        {
            free(result);
        }
        return t;
    }
    else
    {
        return fileName;
    }
}

std::string NeuralAmpModeler::MapFilename(
    const LV2_Feature *const *features,
    const std::string &input,
    const char *browserPath)
{
    if (input.starts_with(this->GetBundlePath().c_str()))
    {
        // map bundle files to corresponding files in the browser dialog directories.
        const LV2_FileBrowser_Files *browserFiles = GetFeature<LV2_FileBrowser_Files>(features, LV2_FILEBROWSER__files);
        if (browserFiles != nullptr)
        {
            char *t = nullptr;
            t = browserFiles->map_path(browserFiles->handle, input.c_str(), "impulseFiles/reverb", browserPath);
            std::string result = t;
            browserFiles->free_path(browserFiles->handle, t);
            return result;
        }
        return input;
    }
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath == nullptr)
    {
        return input;
    }
    else
    {
        char *t = mapPath->absolute_path(mapPath->handle, input.c_str());
        std::string result = t;
        if (freePath)
        {
            freePath->free_path(freePath->handle, t);
        }
        else
        {
            free(t);
        }
        return result;
    }
}

LV2_State_Status
NeuralAmpModeler::OnSaveLv2State(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    if (this->mNAMPath.length() == 0)
    {
        return LV2_State_Status::LV2_STATE_SUCCESS; // not-set => "". Avoids assuming that hosts can handle a "" path.
    }
    std::string abstractPath = this->UnmapFilename(features, this->mNAMPath.c_str());
    store(handle, this->urids.nam__ModelFileName, abstractPath.c_str(), abstractPath.length() + 1, urids.atom__Path, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    return LV2_State_Status::LV2_STATE_SUCCESS;
}

bool NeuralAmpModeler::LoadModel(const std::string &modelFileName)
{
    try
    {
        this->mNAMPath = modelFileName;
        std::unique_ptr<DSP> dspResult;
        if (modelFileName.length() != 0)
        {
            dspResult = _GetNAM(modelFileName);
        }
        this->mNAM = std::move(dspResult);

        if (mNAM)
        {
            PrepareModel(this->mNAM.get());
        }
        else
        {
            if (!mNAMPath.empty())
            {
                std::string fileNameOnly = std::filesystem::path(modelFileName).filename().replace_extension();

                LogError("%s\n", SS("can't load model " << fileNameOnly).c_str());
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::string fileNameOnly = std::filesystem::path(modelFileName).filename().replace_extension();
        LogError("%s\n", SS("can't load model " << fileNameOnly).c_str());
        mNAM = nullptr;
        return false;
    }
}

void NeuralAmpModeler::PrepareModel(DSP *pDSP)
{
    //pDSP->SetNormalize(true);

    // Make the model allocate all of it's Matrices, which aren't initialized
    // until first execution. We don't want to be doing mallocs on the realtime thread.
    int nFrames = (int)this->nominalBlockLength;
    if (nFrames > 128)
        nFrames = 128;

    if (nFrames < 32)
    {
        nFrames = 32;
    }
    std::vector<nam_float_t> outputBuffer(nFrames);

    std::vector<nam_float_t> inputBuffer(nFrames);
    pDSP->process(inputBuffer.data(), outputBuffer.data(), nFrames);
}

LV2_State_Status
NeuralAmpModeler::OnRestoreLv2State(
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    std::string modelFileName;

    {
        size_t size;
        uint32_t type;
        uint32_t flags;
        const void *data = (*retrieve)(handle, urids.nam__ModelFileName, &size, &type, &flags);
        if (data)
        {
            if (type != this->urids.atom__Path && type != this->urids.atom__String)
            {
                return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
            }
            modelFileName = MapFilename(features, (const char *)data, nullptr);
            RequestLoad(modelFileName.c_str());
        }
    }

    return LV2_State_Status::LV2_STATE_SUCCESS;
}

LV2_Worker_Status NeuralAmpModeler::OnWork(
    LV2_Worker_Respond_Function respond,
    LV2_Worker_Respond_Handle handle,
    uint32_t size,
    const void *data)
{
    NamMessage *message = (NamMessage *)data;
    switch (message->MessageType())
    {
    case NamMessageType::Load:
    {
        std::string dspFilename = "";
        std::unique_ptr<DSP> dspResult;
        std::string irFilename = "";

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

        NamLoadMessage *pLoadMessage = static_cast<NamLoadMessage *>(message);
        dspFilename = pLoadMessage->ModelFileName();
        if (!dspFilename.empty())
        {
            std::filesystem::path filename = pLoadMessage->ModelFileName();
            try
            {

                dspResult = _GetNAM(filename);
                dspFilename = filename;
                if (dspResult)
                {
                    PrepareModel(dspResult.get());
                }
                else
                {
                    LogError("%s\n", SS("Can't load model " << filename.filename().replace_extension() << ".").c_str());
                }
            }
            catch (const std::exception &e)
            {
                LogError("%s\n", SS("Can't load model " << filename.filename().replace_extension() << ".").c_str());
            }
        }
        NamLoadResponseMessage reply{
            dspFilename.c_str(),
            dspResult.release()};
        respond(handle, sizeof(reply), &reply);
    }
    break;
    case NamMessageType::FreeLoad:
    {
        NamFreeMessage *pFreeMessage = static_cast<NamFreeMessage *>(message);
        pFreeMessage->Work();
    }
    break;
    default:
        return LV2_Worker_Status::LV2_WORKER_ERR_UNKNOWN;
    }
    return LV2_Worker_Status::LV2_WORKER_SUCCESS;
}

LV2_Worker_Status NeuralAmpModeler::OnWorkResponse(uint32_t size, const void *data)
{
    NamMessage *response = (NamMessage *)data;
    switch (response->MessageType())
    {
    case NamMessageType::LoadResponse:
    {
        NamLoadResponseMessage *loadResponse = (NamLoadResponseMessage *)response;
        DSP *oldModel = nullptr;
        oldModel = this->mNAM.release();

        this->mNAM = std::unique_ptr<DSP>(loadResponse->modelObject);

        if (oldModel != nullptr)
        {
            const LV2_Worker_Schedule *schedule = this->GetLv2WorkerSchedule();
            NamFreeMessage freeMessage{oldModel};
            schedule->schedule_work(schedule->handle, sizeof(freeMessage), &freeMessage);
        }
    }
    break;
    default:
        LogError("Invalid work response.");
        break;
    }
    return LV2_Worker_Status::LV2_WORKER_SUCCESS;
}

void NeuralAmpModeler::ConnectPort(uint32_t port, void *data)
{
    switch ((EParams)port)
    {
    case EParams::kInputGain:
        cInputGain.SetData(data);
        break;
    case EParams::kInputLevelOut:
        cInputLevelOut.SetData(data);
        cInputLevelOut.SetValue(INPUT_LEVEL_MIN);
        break;
    case EParams::kOutputGain:
        cOutputGain.SetData(data);
        break;
    case EParams::kNoiseGateThreshold:
        cNoiseGateThreshold.SetData(data);
        break;
    case EParams::kGateOut:
        cGateOutput.SetData(data);
        break;
    case EParams::kBass:
        cBass.SetData(data);
        break;
    case EParams::kMid:
        cMid.SetData(data);
        break;
    case EParams::kTreble:
        cTreble.SetData(data);
        break;
    case EParams::kStackType:
        cToneStackType.SetData(data);
        break;
    // case EParams::kOutNorm:
    //     cOutNorm.SetData(data);
    //     break;
    case EParams::kAudioIn:
        audioIn = (const float *)data;
        break;
    case EParams::kAudioOut:
        audioOut = (float *)data;
        break;
    case EParams::kControlIn:
        controlIn = (LV2_Atom_Sequence *)data;
        break;
    case EParams::kControlOut:
        controlOut = (LV2_Atom_Sequence *)data;
        break;
    default:
        LogWarning("Invalid ConnectPort call.\n");
        break;
    }
}
void NeuralAmpModeler::Activate()
{
    isActivated = true;

    this->toneStackFilter.Reset();
    this->baxandallToneStack.Reset();

    size_t maxBufferSize = this->GetBuffSizeOptions().maxBlockLength;
    this->nominalBlockLength = this->GetBuffSizeOptions().nominalBlockLength;
    if (maxBufferSize == BufSizeOptions::INVALID_VALUE)
    {
        maxBufferSize = 2048;
    }

    this->_PrepareIOPointers(1);
    this->mInputArray.resize(1);
    this->mOutputArray.resize(1);
    this->_PrepareBuffers(maxBufferSize);

    const double time = 0.01;
    const double threshold = cNoiseGateThreshold.GetDb();
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.02;
    const double closeTime = 0.03;
    dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    this->mNoiseGateTrigger.SetParams(triggerParams);
    this->mNoiseGateTrigger.SetSampleRate(rate);
    this->noiseGateActive = cNoiseGateThreshold.GetDb() != -100;

    this->mNoiseGateTrigger.PrepareBuffers(1, maxBufferSize);
    this->mNoiseGateGain.PrepareBuffers(1, maxBufferSize);

    LoadModel(this->mNAMPath);
}
void NeuralAmpModeler::Run(uint32_t n_samples)
{

    BeginAtomOutput(this->controlOut);
    HandleEvents(this->controlIn);
    ProcessBlock(n_samples);
    if (requestFileUpdate)
    {
        requestFileUpdate = false;
        this->PutPatchPropertyPath(0, urids.nam__ModelFileName, mNAMPath.c_str());
    }
}
void NeuralAmpModeler::Deactivate()
{
    isActivated = false;
}
NeuralAmpModeler::~NeuralAmpModeler()
{
}

void NeuralAmpModeler::UpdateToneStack()
{
    auto previousToneStack = this->toneStackType;

    this->toneStackType = (ToneStackType)cToneStackType.GetValue();
    bool toneStackChanged = this->toneStackType != previousToneStack;

    float b = cBass.GetValue() * 0.1f;
    float m = cMid.GetValue() * 0.1f;
    float t = cTreble.GetValue() * 0.1f;

    // std::cout << "b: " << b << " m: " << m << " t: " << t << std::endl;
    switch (this->toneStackType)
    {
    case ToneStackType::Bassman:
        this->toneStackFilter.UpdateFilter(
            ToneStackFilter::AmpModel::Bassman,
            b, m, t);
        if (toneStackChanged)
        {
            this->toneStackFilter.Reset();
        }
        break;
    case ToneStackType::Jcm8000:
        this->toneStackFilter.UpdateFilter(
            ToneStackFilter::AmpModel::JCM800,
            b, m, t);
        if (toneStackChanged)
        {
            this->toneStackFilter.Reset();
        }
        break;
    case ToneStackType::Baxandall:
        this->baxandallToneStack.Design(b, m, t);
        if (toneStackChanged)
        {
            this->baxandallToneStack.Reset();
        }
        break;
    case ToneStackType::Bypass:
        break;
    }
    // Request an update 15 ms later.
}
void NeuralAmpModeler::ProcessBlock(int nFrames)
{

    constexpr size_t numChannelsInternal = 1;
    const size_t numFrames = (size_t)nFrames;
    const double sampleRate = this->rate;

    // Disable floating point denormals

    fp_state_t fp_state = LsNumerics::disable_denorms();

    this->_PrepareBuffers(numFrames);

    if (cBass.HasChanged() || cMid.HasChanged() || cTreble.HasChanged() || cToneStackType.HasChanged())
    {
        UpdateToneStack();
        // generate a throttled patch_Set
        if (responseDelaySamples == 0)
        {
            responseDelaySamples = responseDelaySamplesMax;
        }
    }

    // Input is collapsed to mono in preparation for the NAM.
    this->_ProcessInput(&this->audioIn, numFrames, 1, 1);

    // Noise gate trigger
    nam_float_t **triggerOutput = mInputPointers;
    if (cNoiseGateThreshold.HasChanged())
    {
        this->noiseGateActive = cNoiseGateThreshold.GetDb() != -100;
        const double time = 0.01;
        const double threshold = cNoiseGateThreshold.GetDb();
        const double ratio = 0.1; // Quadratic...
        const double openTime = 0.005;
        const double holdTime = 0.01;
        const double closeTime = 0.05;
        dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
        this->mNoiseGateTrigger.SetParams(triggerParams);
        this->mNoiseGateTrigger.SetSampleRate(sampleRate);
        if (!noiseGateActive) {
            this->cGateOutput.SetValue(0);
        }

    }
    float noiseGateOut = 1;
    if (noiseGateActive)
    {
        triggerOutput = this->mNoiseGateTrigger.Process(mInputPointers, 1, numFrames);
        noiseGateOut = (float)(this->mNoiseGateTrigger.GetGainReduction()[0][0]);
    }

    nam_float_t **toneStackOutput = triggerOutput;

    switch (this->toneStackType)
    {
    case ToneStackType::Bassman:
    case ToneStackType::Jcm8000:
        toneStackFilter.Process(nFrames, triggerOutput[0], mToneStackPointer);
        toneStackOutput = &this->mToneStackPointer;
        break;
    case ToneStackType::Baxandall:
        baxandallToneStack.Process(nFrames, triggerOutput[0], mToneStackPointer);
        toneStackOutput = &this->mToneStackPointer;
        break;
    case ToneStackType::Bypass:
        break;
    }

    if (mNAM != nullptr)
    {
        // mNAM->SetNormalize(cOutNorm.GetValue());
        // TODO remove input / output gains from here.
        // normalize input.
        mNAM->process(toneStackOutput[0], this->mOutputPointers[0], nFrames);
    }
    else
    {
        this->_FallbackDSP(toneStackOutput, this->mOutputPointers, 1, numFrames);
    }
    // Apply the noise gate
    nam_float_t **gateGainOutput = noiseGateActive
                                       ? this->mNoiseGateGain.Process(this->mOutputPointers, numChannelsInternal, numFrames)
                                       : this->mOutputPointers;

    // Let's get outta here
    // This is where we exit mono for whatever the output requires.
    this->_ProcessOutput(gateGainOutput, &(this->audioOut), numFrames, 1, 1);
    // * Output of input leveling (inputs -> mInputPointers),
    // * Output of output leveling (mOutputPointers -> outputs)

    this->gateOutputUpdateCount += numFrames;
    if (this->gateOutputUpdateCount >= this->gateOutputUpdateRate)
    {
        this->gateOutputUpdateCount = 0;
        this->cGateOutput.SetValue(1 - noiseGateOut);
    }

    if (responseDelaySamples != 0)
    {
        responseDelaySamples -= nFrames;
        if (responseDelaySamples < 0 || nFrames == 0)
        {
            responseGet = true;
            responseDelaySamples = 0;
        }
    }
    if (responseGet)
    {
        responseGet = false;
        responseDelaySamples = 0;
        WriteFrequencyResponse();
    }
    if (sendFileName)
    {
        sendFileName = false;
        this->PutPatchPropertyPath(0, urids.nam__ModelFileName, mNAMPath.c_str());
    }
    // restore previous floating point state
    LsNumerics::restore_denorms(fp_state);
}

void NeuralAmpModeler::OnReset()
{
}

void NeuralAmpModeler::OnIdle()
{
}

// bool NeuralAmpModeler::SerializeState(IByteChunk &chunk) const
// {
//     // Model directory (don't serialize the model itself; we'll just load it again
//     // when we unserialize)
//     chunk.PutStr(this->mNAMPath.Get());
//     chunk.PutStr(this->mIRPath.Get());
//     return SerializeParams(chunk);
// }

// int NeuralAmpModeler::UnserializeState(const IByteChunk &chunk, int startPos)
// {
//     std::string dir;
//     startPos = chunk.GetStr(this->mNAMPath, startPos);
//     startPos = chunk.GetStr(this->mIRPath, startPos);
//     int retcode = UnserializeParams(chunk, startPos);
//     if (this->mNAMPath.length())
//         this->_GetNAM(this->mNAMPath);
//     if (this->mIRPath.length())
//         this->_GetIR(this->mIRPath);
//     return retcode;
// }

// void NeuralAmpModeler::OnUIOpen()
// {
//     Plugin::OnUIOpen();

//     if (this->mNAMPath.length())
//         SendControlMsgFromDelegate(
//             kCtrlTagModelFileBrowser, kMsgTagLoadedModel, this->mNAMPath.length(), this->mNAMPath.Get());
//     if (this->mIRPath.length())
//         SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, this->mIRPath.length(), this->mIRPath.Get());
//     if (this->mNAM != nullptr)
//         this->GetUI()->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(!this->mNAM->HasLoudness());
// }

// void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
// {
//     if (auto pGraphics = GetUI())
//     {
//         bool active = GetParam(paramIdx)->Bool();

//         switch (paramIdx)
//         {
//         case kNoiseGateActive:
//             pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active);
//             break;
//         case kEQActive:
//             pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl *pControl)
//                                          { pControl->SetDisabled(!active); });
//             break;
//         case kIRToggle:
//             pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active);
//         default:
//             break;
//         }
//     }
// }

// Private methods ============================================================

void NeuralAmpModeler::_FallbackDSP(nam_float_t **inputs, nam_float_t **outputs, const size_t numChannels,
                                    const size_t numFrames)
{
    for (size_t c = 0; c < numChannels; c++)
        for (size_t s = 0; s < numFrames; s++)
            outputs[c][s] = inputs[c][s];
}

std::unique_ptr<DSP> NeuralAmpModeler::_GetNAM(const std::string &modelPath)
{
    if (modelPath.length() == 0)
    {
        return nullptr;
    }
    auto dspPath = std::filesystem::u8path(modelPath);
    std::unique_ptr<DSP> nam = get_dsp_ex(dspPath,(int)(this->GetBuffSizeOptions().minBlockLength),(int)(this->GetBuffSizeOptions().maxBlockLength));
    return nam;
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
    // Assumes input=output (no mono->stereo effects)
    return this->mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
    if (this->_GetBufferNumChannels() == 0)
        return 0;
    return this->mInputArray[0].size();
}

void NeuralAmpModeler::_PrepareBuffers(const size_t numFrames)
{
    if (mInputArray.size() == 0 || mInputArray[0].size() >= numFrames)
    {
        return;
    }
    {
        mToneStackArray.resize(numFrames);
        mToneStackPointer = &(mToneStackArray[0]);

        for (size_t c = 0; c < this->mInputArray.size(); c++)
        {
            this->mInputArray[c].resize(numFrames);
        }
        for (size_t c = 0; c < this->mOutputArray.size(); c++)
        {
            this->mOutputArray[c].resize(numFrames);
        }
        // Would these ever get changed by something?
        for (size_t c = 0; c < this->mInputArray.size(); c++)
            this->mInputPointerMemory[c] = this->mInputArray[c].data();
        for (size_t c = 0; c < this->mOutputArray.size(); c++)
            this->mOutputPointerMemory[c] = this->mOutputArray[c].data();
    }
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
    this->mInputPointerMemory.resize(numChannels);
    this->mInputPointers = numChannels == 0 ? nullptr : &(mInputPointerMemory[0]);

    this->mOutputPointerMemory.resize(numChannels);
    this->mOutputPointers = numChannels == 0 ? nullptr : &(mOutputPointerMemory[0]);
}

void NeuralAmpModeler::_ProcessInput(const float_t **inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
    // We'll assume that the main processing is mono for now. We'll handle dual amps later.
    // See also: this->mNUM_INTERNAL_CHANNELS
    if (nChansOut != 1)
    {
        std::stringstream ss;
        ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
        throw std::runtime_error(ss.str());
    }

    // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
    // carried straight through. Don't apply any division over nCahnsIn because we're just "catching anything out there."
    // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
    // doubling the loudness.
    const double gain = this->cInputGain.GetAf() / nChansIn;

    // Assume _PrepareBuffers() was already called
    if (nChansIn > 0)
    {
        for (size_t s = 0; s < nFrames; s++)
        {
            this->mInputArray[0][s] = gain * inputs[0][s];
        }
    }
    for (size_t c = 1; c < nChansIn; c++)
    {
        for (size_t s = 0; s < nFrames; s++)
        {
            this->mInputArray[0][s] += gain * inputs[c][s];
        }
    }
    float vuValue = this->vuValue;
    for (size_t i = 0; i < nFrames; ++i)
    {
        float v = std::abs(this->mInputArray[0][i]);
        if (v > vuValue)
            vuValue = v;
    }
    this->vuValue = vuValue;
    this->vuSampleCount += nFrames;
    if (this->vuSampleCount >= this->vuMaxSampleCount)
    {
        float vuDb;
        if (this->vuValue == 0)
        {
            vuDb = INPUT_LEVEL_MIN;
        }
        else
        {
            vuDb = Af2Db(this->vuValue);
            if (vuDb > INPUT_LEVEL_MAX)
                vuDb = INPUT_LEVEL_MAX;
            if (vuDb < INPUT_LEVEL_MIN)
                vuDb = INPUT_LEVEL_MIN;
        }
        cInputLevelOut.SetValue(vuDb);
        this->vuSampleCount = 0;
        this->vuValue = 0;
    }
}

void NeuralAmpModeler::_ProcessOutput(nam_float_t **inputs, float_t **outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
    const float gain = this->cOutputGain.GetAf();
    // Assume _PrepareBuffers() was already called
    if (nChansIn != 1)
        throw std::runtime_error("Plugin is supposed to process in mono.");
    // Broadcast the internal mono stream to all output channels.
    const size_t cin = 0;
    for (size_t cout = 0; cout < nChansOut; cout++)
        for (size_t s = 0; s < nFrames; s++)
#ifdef APP_API // Ensure valid output to interface
            outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
            outputs[cout][s] = gain * inputs[cin][s];
#endif
}

void NeuralAmpModeler::OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *value)
{

    if (propertyUrid == urids.nam__ModelFileName && (value->type == urids.atom__Path || value->type == urids.atom__String))
    {
        const char *modelFileName = ((const char *)value) + sizeof(LV2_Atom);
        RequestLoad(modelFileName);
    }
}
void NeuralAmpModeler::OnPatchGet(LV2_URID propertyUrid)
{
    if (propertyUrid == this->urids.nam__ModelFileName)
    {
        this->sendFileName = true;
    }
    else if (propertyUrid == this->urids.nam__FrequencyResponse)
    {
        this->responseGet = true;
    }
}

float NeuralAmpModeler::CalculateFrequencyResponse(float f)
{
    switch (this->toneStackType)
    {
    case ToneStackType::Bassman:
    case ToneStackType::Jcm8000:
        return toneStackFilter.GetFrequencyResponse(f);
    case ToneStackType::Baxandall:
        return baxandallToneStack.GetFrequencyResponse(f);
    default:
        return 0;
    }
}

void NeuralAmpModeler::WriteFrequencyResponse()
{
    for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
    {
        filterResponse.SetResponse(
            i,
            this->CalculateFrequencyResponse(
                filterResponse.GetFrequency(i)));
    }

    lv2_atom_forge_frame_time(&outputForge, 0);

    LV2_Atom_Forge_Frame objectFrame;
    lv2_atom_forge_object(&outputForge, &objectFrame, 0, urids.patch__Set);

    lv2_atom_forge_key(&outputForge, urids.patch__property);
    lv2_atom_forge_urid(&outputForge, urids.nam__FrequencyResponse);
    lv2_atom_forge_key(&outputForge, urids.patch__value);

    LV2_Atom_Forge_Frame vectorFrame;
    lv2_atom_forge_vector_head(&outputForge, &vectorFrame, sizeof(float), urids.atom__float);
    if (this->toneStackType == ToneStackType::Baxandall)
    {
        lv2_atom_forge_float(&outputForge, 30.0f);
        lv2_atom_forge_float(&outputForge, 20000.0f);
        lv2_atom_forge_float(&outputForge, 20.0f);
        lv2_atom_forge_float(&outputForge, -20.0f);
    }
    else
    {
        lv2_atom_forge_float(&outputForge, 30.0f);
        lv2_atom_forge_float(&outputForge, 20000.0f);
        lv2_atom_forge_float(&outputForge, 5.0f);
        lv2_atom_forge_float(&outputForge, -30.0f);
    }
    for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
    {
        // lv2_atom_forge_float(&outputForge,filterResponse.GetFrequency(i));
        lv2_atom_forge_float(&outputForge, filterResponse.GetResponse(i));
    }
    lv2_atom_forge_pop(&outputForge, &vectorFrame);

    lv2_atom_forge_pop(&outputForge, &objectFrame);
}

void NeuralAmpModeler::RequestLoad(const char *fileName)
{
    this->mNAMPath = fileName;
    this->sendFileName = true;
    if (!this->isActivated)
    {
        // will be picked up in Activate.
        this->mNAMPath = fileName;
        this->sendFileName = fileName;
        return;
    }

    const LV2_Worker_Schedule *schedule = GetLv2WorkerSchedule();
    if (schedule)
    {
        this->mNAMPath = fileName;
        this->sendFileName = true;

        NamLoadMessage loadMessage(fileName);

        schedule->schedule_work(
            schedule->handle,
            sizeof(loadMessage), &loadMessage); // must be POD!
    }
    else
    {
        LoadModel(fileName); // do it on the foreground.
    }
}
//----------------------------------------------
