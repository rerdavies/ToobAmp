/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:

 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.

 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */
#include "ToobConvolutionReverb.h"

#include "db.h"
#include <thread>
#include "WavReader.hpp"
#include "FlacReader.hpp"
#include "ss.hpp"
#include "LsNumerics/BalancedConvolution.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "ss.hpp"

#define TOOB_CONVOLUTION_REVERB_URI "http://two-play.com/plugins/toob-convolution-reverb"
#define TOOB_CAB_IR_URI "http://two-play.com/plugins/toob-cab-ir"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

using namespace toob;

const float MAX_DELAY_MS = 4000;
const float NOMINAL_DELAY_MS = 1600;

constexpr float MIN_MIX_DB = -40;

ToobConvolutionReverb::ToobConvolutionReverb(
    bool isConvolutionReverb_,
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2PluginWithState(features),
      sampleRate(rate),
      bundle_path(bundle_path),
      loadWorker(this)

{
    this->isConvolutionReverb = isConvolutionReverb_;
    urids.Init(this);
    loadWorker.Initialize((size_t)rate, this);

    SetDefaultFile(features);
    std::filesystem::path planFilePath{bundle_path};
    planFilePath = planFilePath / "fftplans";
    BalancedConvolutionSection::SetPlanFileDirectory(planFilePath.string());
    try
    {
        PublishResourceFiles(features);
    }
    catch (const std::exception &e)
    {
        LogWarning(e.what());
    }
}

const char *ToobConvolutionReverb::CONVOLUTION_REVERB_URI = TOOB_CONVOLUTION_REVERB_URI;
const char *ToobConvolutionReverb::CAB_IR_URI = TOOB_CAB_IR_URI;

void ToobConvolutionReverb::ConnectPort(uint32_t port, void *data)
{
    if (IsConvolutionReverb())
    {
        switch ((ReverbPortId)port)
        {
        case ReverbPortId::TIME:
            this->pTime = (float *)data;
            break;
        case ReverbPortId::DIRECT_MIX:
            this->pDirectMix = (float *)data;
            break;
        case ReverbPortId::REVERB_MIX:
            this->pReverbMix = (float *)data;
            break;
        case ReverbPortId::REVERB2_MIX:
            this->pReverb2Mix = (float *)data;
            break;
        case ReverbPortId::REVERB3_MIX:
            this->pReverb3Mix = (float *)data;
            break;

        case ReverbPortId::PREDELAY:
            this->pPredelay = (float *)data;
            break;
        case ReverbPortId::LOADING_STATE:
            this->pLoadingState = (float *)data;
            if (this->pLoadingState)
            {
                *(this->pLoadingState) = this->loadingState;
            }
            break;

        case ReverbPortId::AUDIO_INL:
            this->inL = (const float *)data;
            break;
        case ReverbPortId::AUDIO_OUTL:
            this->outL = (float *)data;
            break;
        case ReverbPortId::CONTROL_IN:
            this->controlIn = (LV2_Atom_Sequence *)data;
            break;
        case ReverbPortId::CONTROL_OUT:
            this->controlOut = (LV2_Atom_Sequence *)data;
            break;
        }
    }
    else
    {
        switch ((CabIrPortId)port)
        {
        case CabIrPortId::TIME:
            this->pTime = (float *)data;
            break;
        case CabIrPortId::DIRECT_MIX:
            this->pDirectMix = (float *)data;
            break;
        case CabIrPortId::REVERB_MIX:
            this->pReverbMix = (float *)data;
            break;
        case CabIrPortId::REVERB2_MIX:
            this->pReverb2Mix = (float *)data;
            break;
        case CabIrPortId::REVERB3_MIX:
            this->pReverb3Mix = (float *)data;
            break;

        case CabIrPortId::PREDELAY:
            this->pPredelay = (float *)data;
            break;
        case CabIrPortId::LOADING_STATE:
            this->pLoadingState = (float *)data;
            if (this->pLoadingState)
            {
                *(this->pLoadingState) = this->loadingState;
            }
            break;

        case CabIrPortId::AUDIO_INL:
            this->inL = (const float *)data;
            break;
        case CabIrPortId::AUDIO_OUTL:
            this->outL = (float *)data;
            break;
        case CabIrPortId::CONTROL_IN:
            this->controlIn = (LV2_Atom_Sequence *)data;
            break;
        case CabIrPortId::CONTROL_OUT:
            this->controlOut = (LV2_Atom_Sequence *)data;
            break;
        }
    }
}
void ToobConvolutionReverb::clear()
{
    if (pConvolutionReverb)
    {
        // pConvolutionReverb->Reset();
    }
}

void ToobConvolutionReverb::UpdateControls()
{
    if (lastTime != *pTime)
    {
        lastTime = *pTime;
        time = lastTime;
        loadWorker.SetTime(time);
    }
    if (lastDirectMix != *pDirectMix)
    {
        lastDirectMix = *pDirectMix;
        if (lastDirectMix <= MIN_MIX_DB)
        {
            directMixAf = 0;
        }
        else
        {
            directMixAf = db2a(lastDirectMix);
        }
        if (pConvolutionReverb)
        {
            pConvolutionReverb->SetDirectMix(directMixAf);
        }
    }
    if (lastReverbMix != *pReverbMix)
    {
        lastReverbMix = *pReverbMix;
        if (lastReverbMix <= MIN_MIX_DB)
        {
            reverbMixAf = 0;
        }
        else
        {
            reverbMixAf = db2a(lastReverbMix);
        }
        if (this->IsConvolutionReverb())
        {
            if (!loadWorker.IsChanging())
            {
                pConvolutionReverb->SetReverbMix(reverbMixAf);
            }
        }
        else
        {
            loadWorker.SetMix(reverbMixAf);
        }
    }
    if (lastReverb2Mix != *pReverb2Mix)
    {
        lastReverb2Mix = *pReverb2Mix;
        if (lastReverb2Mix <= MIN_MIX_DB)
        {
            reverb2MixAf = 0;
        }
        else
        {
            reverb2MixAf = db2a(lastReverb2Mix);
        }
        loadWorker.SetMix2(reverb2MixAf);
    }
    if (lastReverb3Mix != *pReverb3Mix)
    {
        lastReverb3Mix = *pReverb3Mix;
        if (lastReverb3Mix <= MIN_MIX_DB)
        {
            reverb3MixAf = 0;
        }
        else
        {
            reverb3MixAf = db2a(lastReverb3Mix);
        }
        loadWorker.SetMix3(reverb3MixAf);
    }

    if (lastPredelay != *pPredelay)
    {
        lastPredelay = *pPredelay;
        loadWorker.SetPredelay(lastPredelay != 0);
    }
}
void ToobConvolutionReverb::Activate()
{
    activated = true;
    lastReverbMix = lastDirectMix = lastTime = std::numeric_limits<float>::min(); // force updates
    UpdateControls();

    clear();
}

void ToobConvolutionReverb::Run(uint32_t n_samples)
{
    BeginAtomOutput(this->controlOut);
    HandleEvents(this->controlIn);
    UpdateControls();
    if (n_samples != 0) // prevent acccidentally triggering heavy work during pre-load.
    {
        if (loadWorker.Changed() && loadWorker.IsIdle())
        {
            if (!preChangeVolumeZip)
            {
                if (pConvolutionReverb)
                {
                    preChangeVolumeZip = true;
                    pConvolutionReverb->SetDirectMix(0);
                    pConvolutionReverb->SetReverbMix(0);
                }
            }
            if ((!pConvolutionReverb) || (!pConvolutionReverb->IsDezipping()))
            {
                preChangeVolumeZip = false;
                loadWorker.Tick();
            }
        }
        if (pConvolutionReverb)
        {
            pConvolutionReverb->Tick(n_samples, inL, outL);
        }
        else
        {
            for (uint32_t i = 0; i < n_samples; ++i)
            {
                this->outL[i] = 0;
            }
        }
    }
    EndAtomOutput();
}

void ToobConvolutionReverb::CancelLoad()
{
}
void ToobConvolutionReverb::Deactivate()
{
    CancelLoad();
}

std::string ToobConvolutionReverb::StringFromAtomPath(const LV2_Atom *atom)
{
    // LV2 declaration is insufficient to locate the body.
    typedef struct
    {
        LV2_Atom atom;   /**< Atom header. */
        const char c[1]; /* Contents (a null-terminated UTF-8 string) follow here. */
    } LV2_Atom_String_x;

    const char *p = ((const LV2_Atom_String_x *)atom)->c;
    size_t size = atom->size;
    while (size > 0 && p[size - 1] == 0)
    {
        --size;
    }
    return std::string(p, size);
}
void ToobConvolutionReverb::OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *atom)
{
    // LV2 declaration is insufficient to locate the body.

    if (propertyUrid == urids.reverb__propertyFileName || propertyUrid == urids.cabir__propertyFileName)
    {
        std::string name = StringFromAtomPath(atom);

        bool changed = loadWorker.SetFileName(name.c_str());
        if (changed)
        {
            PutStateChanged(0);
        }
    }
    if (propertyUrid == urids.cabir__propertyFileName2)
    {
        std::string name = StringFromAtomPath(atom);

        bool changed = loadWorker.SetFileName2(name.c_str());
        if (changed)
        {
            PutStateChanged(0);
        }
    }
    if (propertyUrid == urids.cabir__propertyFileName3)
    {
        std::string name = StringFromAtomPath(atom);

        bool changed = loadWorker.SetFileName3(name.c_str());
        if (changed)
        {
            PutStateChanged(0);
        }
    }
}

void ToobConvolutionReverb::OnPatchGetAll()
{
    // PutPatchPropertyPath(urids.propertyFileName, urids.propertyFileName, this->loadWorker.GetFileName());
}
void ToobConvolutionReverb::OnPatchGet(LV2_URID propertyUrid)
{
    if (propertyUrid == urids.reverb__propertyFileName)
    {
        PutPatchPropertyPath(0, urids.reverb__propertyFileName, this->loadWorker.GetFileName());
    }
    if (propertyUrid == urids.cabir__propertyFileName)
    {
        PutPatchPropertyPath(0, urids.cabir__propertyFileName, this->loadWorker.GetFileName());
    }
    else if (propertyUrid == urids.cabir__propertyFileName2)
    {
        PutPatchPropertyPath(0, urids.cabir__propertyFileName2, this->loadWorker.GetFileName2());
    }
    else if (propertyUrid == urids.cabir__propertyFileName3)
    {
        PutPatchPropertyPath(0, urids.cabir__propertyFileName3, this->loadWorker.GetFileName3());
    }
}

ToobConvolutionReverb::LoadWorker::LoadWorker(Lv2Plugin *pPlugin)
    : base(pPlugin)
{
    pThis = dynamic_cast<ToobConvolutionReverb *>(pPlugin);
    memset(fileName, 0, sizeof(fileName));
    memset(fileName2, 0, sizeof(fileName));
    memset(fileName3, 0, sizeof(fileName));
    memset(requestFileName, 0, sizeof(requestFileName));
    memset(requestFileName2, 0, sizeof(requestFileName));
    memset(requestFileName3, 0, sizeof(requestFileName));
}

void ToobConvolutionReverb::LoadWorker::Initialize(size_t sampleRate, ToobConvolutionReverb *pReverb)
{
    this->sampleRate = sampleRate;
    this->pReverb = pReverb;
}

bool ToobConvolutionReverb::LoadWorker::SetTime(float timeInSeconds)
{
    if (this->timeInSeconds != timeInSeconds)
    {
        this->timeInSeconds = timeInSeconds;
        this->changed = true;
        return true;
    }
    return false;
}
bool ToobConvolutionReverb::LoadWorker::SetPredelay(bool usePredelay)
{
    if (this->predelay != usePredelay)
    {
        this->predelay = usePredelay;
        this->changed = true;
        return true;
    }
    return false;
}
bool ToobConvolutionReverb::LoadWorker::SetFileName(const char *szName)
{
    size_t length = strlen(szName);
    if (length >= sizeof(fileName) - 1)
    {
        pReverb->LogError("File name too long.");
        SetState(State::Error);
        return false;
    }

    bool changed = strncmp(fileName, szName, sizeof(fileName) - 1) != 0;
    if (!changed)
        return false;
    this->changed = true;
    strncpy(fileName, szName, sizeof(fileName) - 1);
    return true;
}

bool ToobConvolutionReverb::LoadWorker::SetFileName2(const char *szName)
{
    size_t length = strlen(szName);
    if (length >= sizeof(fileName) - 1)
    {
        pReverb->LogError("File name too long.");
        SetState(State::Error);
        return false;
    }

    bool changed = strncmp(fileName2, szName, sizeof(fileName2) - 1) != 0;
    if (!changed)
        return false;
    this->changed = true;
    strncpy(fileName2, szName, sizeof(fileName2) - 1);
    return true;
}
bool ToobConvolutionReverb::LoadWorker::SetFileName3(const char *szName)
{
    size_t length = strlen(szName);
    if (length >= sizeof(fileName) - 1)
    {
        pReverb->LogError("File name too long.");
        SetState(State::Error);
        return false;
    }

    bool changed = strncmp(fileName3, szName, sizeof(fileName3) - 1) != 0;
    if (!changed)
        return false;
    this->changed = true;
    strncpy(fileName3, szName, sizeof(fileName3) - 1);
    return true;
}

bool ToobConvolutionReverb::LoadWorker::SetMix(float value)
{
    if (value != this->mix)
    {
        this->mix = value;
        this->changed = true;
        return true;
    }
    return false;
}
bool ToobConvolutionReverb::LoadWorker::SetMix2(float value)
{
    if (value != this->mix2)
    {
        this->mix2 = value;
        this->changed = true;
        return true;
    }
    return false;
}
bool ToobConvolutionReverb::LoadWorker::SetMix3(float value)
{
    if (value != this->mix3)
    {
        this->mix3 = value;
        this->changed = true;
        return true;
    }
    return false;
}
void ToobConvolutionReverb::LoadWorker::SetState(State state)
{
    if (this->state != state)
    {
        this->state = state;
        pReverb->SetLoadingState((int)state);
    }
}
void ToobConvolutionReverb::LoadWorker::Request()
{
    // make a copoy for thread safety.
    strncpy(requestFileName, fileName, sizeof(requestFileName));
    strncpy(requestFileName2, fileName2, sizeof(requestFileName2));
    strncpy(requestFileName3, fileName3, sizeof(requestFileName3));
    this->requestMix = this->mix;
    this->requestMix2 = this->mix2;
    this->requestMix3 = this->mix3;

    SetState(State::SentRequest);

    // take the existing convolution reverb off the main thread.
    this->oldConvolutionReverb = std::move(pReverb->pConvolutionReverb);
    this->workingPredelay = predelay; // capture a copy
    this->workingTimeInSeconds = this->timeInSeconds;

    WorkerAction::Request();
}

static void NormalizeConvolution(AudioData &data)
{
    size_t size = data.getSize();

    for (size_t c = 0; c < data.getChannelCount(); ++c)
    {
        auto &channel = data.getChannel(c);
        double maxValue = 0;
        // find the worst-case convolution output.
        double sum = 0;
        for (size_t i = 0; i < size; ++i)
        {
            sum += channel[i];
            if (std::abs(sum) > maxValue)
            {
                maxValue = std::abs(sum);
            }
        }
        // std::cout << "MaxValue: " << maxValue << std::endl;

        float scale = (float)(1 / maxValue);

        for (size_t i = 0; i < size; ++i)
        {
            channel[i] *= scale;
        }
    }
}

static void RemovePredelay(AudioData &audioData)
{
    std::vector<float> &channel = audioData.getChannel(0);
    float db60 = LsNumerics::Db2Af(-60);
    float db40 = LsNumerics::Db2Af(-40);

    size_t db60Index = 0;
    size_t db30Index = 0;
    bool seenDb60 = false;
    for (size_t i = 0; i < channel.size(); ++i)
    {
        float value = std::abs(channel[i]);
        if (value > db40)
        {
            db30Index = i;
            break;
        }
        if (value < db60 && !seenDb60)
        {
            db60Index = i;
        }
        else
        {
            seenDb60 = true;
        }
    }
    if (db30Index == 0)
    {
        return;
    }
    constexpr size_t MAX_LEADIN = 30;
    if (db30Index - db60Index > MAX_LEADIN)
    {
        db60Index = db30Index - MAX_LEADIN;
    }
    for (size_t i = db60Index; i < db30Index; ++i)
    {
        // ramped leadin.
        float blend = (i - db60Index) / (float)(db30Index - db60Index);
        channel[i] *= blend;
    }
    // std::cout << "Removing predelay. db60Index: " << db60Index << " db30Index: " << db30Index << std::endl;
    audioData.Erase(0, db60Index);
}
static float GetTailScale(const std::vector<float> &data, size_t tailPosition)
{
    double max = 0;
    double val = 0;
    for (size_t i = tailPosition; i < data.size(); ++i)
    {
        val = std::abs(data[i]);
        if (val > max)
        {
            max = val;
        }
    }
    if (max < 1E-7)
    {
        max = 0;
    }
    return (float)max;
}

AudioData ToobConvolutionReverb::LoadWorker::LoadFile(const std::filesystem::path &fileName, float level)
{
    if (fileName.string().length() == 0)
    {
        return AudioData(pReverb->getSampleRate(), 1, 0);
    }
    AudioData data;
    if (fileName.extension() == ".flac")
    {
        data = FlacReader::Load(fileName);
    }
    else
    {
        data = WavReader::Load(fileName);
    }

    // Assume files with 4 channels are in Ambisonic b-Format.
    if (data.getChannelCount() == 4)
    {
        data.AmbisonicDownmix({AmbisonicMicrophone(0, 0)});
    }
    else
    {
        data.ConvertToMono();
    }
    pThis->LogNote(SS("File loaded. Sample rate: " << data.getSampleRate() << std::setprecision(3) << " Length: " << (data.getSize() * 1.0f / data.getSampleRate()) << "s.").c_str());

    NormalizeConvolution(data);
    if (!predelay) // bbetter to do it on the pristine un-filtered data.
    {
        RemovePredelay(data);
    }
    data.Resample((size_t)pReverb->getSampleRate());

    NormalizeConvolution(data);

    data.Scale(level);

    return data;
}
void ToobConvolutionReverb::LoadWorker::OnWork()
{
    // non-audio thread. Memory allocations are allowed!

    this->oldConvolutionReverb = nullptr; // destroy the old convolution reverb if it exists.

    pThis->LogNote("%s", SS("Loading " << requestFileName).c_str());
    hasWorkError = false;
    workError = "";
    try
    {
        AudioData data = LoadFile(requestFileName, requestMix);
        if (requestFileName2[0])
        {
            AudioData data2 = LoadFile(requestFileName2, requestMix2);
            data += data2;
        }
        if (this->requestFileName3[0])
        {
            AudioData data3 = LoadFile(requestFileName3, requestMix3);
            data += data3;
        }
        size_t maxSize = (size_t)std::ceil(workingTimeInSeconds * pReverb->getSampleRate());
        this->tailScale = 0;
        if (maxSize < data.getSize())
        {
            this->tailScale = GetTailScale(data.getChannel(0), maxSize);
            data.setSize(maxSize);

            pThis->LogNote(SS("Max T: " << std::setprecision(3) << workingTimeInSeconds << "s Feedback: " << tailScale).c_str());

        } 
        if (data.getSize() == 0)
        {
            data.setSize(1);
        }
        this->convolutionReverbResult = std::make_shared<ConvolutionReverb>(SchedulerPolicy::Realtime, data.getSize(), data.getChannel(0));
        if (tailScale != 0)
        {
        }
        this->convolutionReverbResult->SetFeedback(tailScale, data.getSize() - 1);
        pThis->LogNote("Load complete.");
    }
    catch (const std::exception &e)
    {
        hasWorkError = true;
        workError = e.what();
        workError.c_str(); // allocate zero if neccessary.
    }
}
void ToobConvolutionReverb::LoadWorker::OnResponse()
{
    if (hasWorkError)
    {
        pReverb->LogError(workError.c_str());
    }
    else
    {
        convolutionReverbResult->SetSampleRate(this->sampleRate);
        convolutionReverbResult->ResetDirectMix(0);
        convolutionReverbResult->ResetReverbMix(0);

        convolutionReverbResult->SetDirectMix(pReverb->directMixAf);

        if (pReverb->IsConvolutionReverb())
        {
            convolutionReverbResult->SetReverbMix(pReverb->reverbMixAf);
        }
        else
        {
            convolutionReverbResult->SetReverbMix(1);
        }
        pReverb->pConvolutionReverb = std::move(convolutionReverbResult);
        // pConvolutionReverb now contains the old convolution, which we must dispose of
        // off the audio thread.
    }
    SetState(State::CleaningUp);
}

void ToobConvolutionReverb::LoadWorker::OnCleanup()
{
    this->convolutionReverbResult = nullptr; // actual result was std::swapped onto the main thread.
}
void ToobConvolutionReverb::LoadWorker::OnCleanupComplete()
{
    if (this->hasWorkError)
    {
        SetState(State::Error);
    }
    else
    {
        SetState(State::Idle);
    }
}

template <typename T>
static const T *GetFeature(const LV2_Feature *const *features, const char *featureUri)
{
    while (*features != nullptr)
    {
        if (strcmp((*features)->URI, featureUri) == 0)
        {
            return (const T *)((*features)->data);
        }
        ++features;
    }
    return nullptr;
}

static std::string UnmapPath(const LV2_State_Map_Path *mapPath, const LV2_State_Free_Path *freePath, const std::string &fileName)
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
LV2_State_Status
ToobConvolutionReverb::OnSaveLv2State(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{

    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath == nullptr)
    {
        return LV2_State_Status::LV2_STATE_ERR_NO_FEATURE;
    }
    if (IsConvolutionReverb())

    {
        std::string fileName = UnmapPath(mapPath, freePath, this->loadWorker.GetFileName());
        auto status = store(handle,
                            urids.reverb__propertyFileName,
                            fileName.c_str(),
                            fileName.length() + 1,
                            urids.atom_path,
                            LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
        if (status != LV2_State_Status::LV2_STATE_SUCCESS)
        {
            return status;
        }
    }
    else
    {
        {
            std::string fileName = UnmapPath(mapPath, freePath, this->loadWorker.GetFileName());
            auto status = store(handle,
                                urids.cabir__propertyFileName,
                                fileName.c_str(),
                                fileName.length() + 1,
                                urids.atom_path,
                                LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
            if (status != LV2_State_Status::LV2_STATE_SUCCESS)
            {
                return status;
            }
        }
        {
            std::string fileName = UnmapPath(mapPath, freePath, this->loadWorker.GetFileName2());
            auto status = store(handle,
                                urids.cabir__propertyFileName2,
                                fileName.c_str(),
                                fileName.length() + 1,
                                urids.atom_path,
                                LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
            if (status != LV2_State_Status::LV2_STATE_SUCCESS)
            {
                return status;
            }
        }
        {
            std::string fileName = UnmapPath(mapPath, freePath, this->loadWorker.GetFileName3());
            auto status = store(handle,
                                urids.cabir__propertyFileName3,
                                fileName.c_str(),
                                fileName.length() + 1,
                                urids.atom_path,
                                LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
            if (status != LV2_State_Status::LV2_STATE_SUCCESS)
            {
                return status;
            }
        }
    }
    return LV2_State_Status::LV2_STATE_SUCCESS;
}

LV2_State_Status ToobConvolutionReverb::GetUserResourcePath(const LV2_Feature *const *features, std::filesystem::path *path)
{
    const LV2_State_Make_Path *makePath = GetFeature<LV2_State_Make_Path>(features, LV2_STATE__makePath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (makePath == nullptr)
    {
        LogError("Can't load state. Missing LV2_STATE__makePath feature.");
        return LV2_State_Status::LV2_STATE_ERR_NO_FEATURE;
    }

    // Use makePath to get a user-modifiable directory.
    std::filesystem::path targetPath =
        IsConvolutionReverb()
            ? std::filesystem::path("ReverbImpulseFiles")
            : std::filesystem::path("CabIR");
    {
        char *mappedPath = makePath->path(makePath->handle, targetPath.c_str());
        targetPath = mappedPath;
        if (freePath)
        {
            freePath->free_path(freePath->handle, mappedPath);
        }
        else
        {
            free((void *)mappedPath);
        }
        *path = std::move(targetPath);
    }
    return LV2_State_Status::LV2_STATE_SUCCESS;
}
LV2_State_Status ToobConvolutionReverb::PublishResourceFiles(
    const LV2_Feature *const *features)
{
    std::filesystem::path targetPath;
    LV2_State_Status res = GetUserResourcePath(features, &targetPath);
    if (res != LV2_State_Status::LV2_STATE_SUCCESS)
    {
        return res;
    }
    this->MaybeCreateSampleDirectory(targetPath);
    return LV2_State_Status::LV2_STATE_SUCCESS;
}
// State extension callbacks.
LV2_State_Status
ToobConvolutionReverb::OnRestoreLv2State(
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    size_t size;
    uint32_t type;
    uint32_t myFlags;

    LV2_State_Status result = PublishResourceFiles(features);
    if (result != LV2_State_Status::LV2_STATE_SUCCESS)
    {
        return result;
    }

    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath == nullptr)
    {
        this->LogError("Missing LV2_STATE__mapPath feature. Can't restore state.");
        return LV2_State_Status::LV2_STATE_ERR_NO_FEATURE;
    }

    if (IsConvolutionReverb())
    {
        const void *data = retrieve(
            handle, urids.reverb__propertyFileName, &size, &type, &myFlags);
        if (data)
        {
            if (type != this->urids.atom_path)
            {
                return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
            }
            std::string input((const char *)data, size);
            char *absolutePath = mapPath->absolute_path(mapPath->handle, input.c_str());
            this->loadWorker.SetFileName(absolutePath);
            if (freePath)
            {
                freePath->free_path(freePath->handle, absolutePath);
            }
            else
            {
                free((void *)absolutePath);
            }
        }
        else
        {

            SetDefaultFile(features);
            std::filesystem::path targetPath;
            LV2_State_Status res = GetUserResourcePath(features, &targetPath);
            if (res != LV2_State_Status::LV2_STATE_SUCCESS)
            {
                return res;
            }

            std::filesystem::path defaultFilePath = targetPath / "Genesis 6 Studio Live Room.wav";
            this->loadWorker.SetFileName(defaultFilePath.c_str());
        }
    }
    else
    {
        {
            const void *data = retrieve(
                handle, urids.cabir__propertyFileName, &size, &type, &myFlags);
            if (data)
            {
                if (type != this->urids.atom_path)
                {
                    return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
                }
                std::string input((const char *)data, size);

                char *absolutePath = mapPath->absolute_path(mapPath->handle, input.c_str());
                this->loadWorker.SetFileName(absolutePath);
                if (freePath)
                {
                    freePath->free_path(freePath->handle, absolutePath);
                }
                else
                {
                    free((void *)absolutePath);
                }
            }
            else
            {
                this->loadWorker.SetFileName("");
            }
        }
        {
            const void *data = retrieve(
                handle, urids.cabir__propertyFileName2, &size, &type, &myFlags);
            if (data)
            {
                if (type != this->urids.atom_path)
                {
                    return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
                }
                std::string input((const char *)data, size);

                char *absolutePath = mapPath->absolute_path(mapPath->handle, input.c_str());
                this->loadWorker.SetFileName2(absolutePath);
                if (freePath)
                {
                    freePath->free_path(freePath->handle, absolutePath);
                }
                else
                {
                    free((void *)absolutePath);
                }
            }
            else
            {
                this->loadWorker.SetFileName2("");
            }
        }
        {
            const void *data = retrieve(
                handle, urids.cabir__propertyFileName3, &size, &type, &myFlags);
            if (data)
            {
                if (type != this->urids.atom_path)
                {
                    return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
                }
                std::string input((const char *)data, size);

                char *absolutePath = mapPath->absolute_path(mapPath->handle, input.c_str());
                this->loadWorker.SetFileName3(absolutePath);
                if (freePath)
                {
                    freePath->free_path(freePath->handle, absolutePath);
                }
                else
                {
                    free((void *)absolutePath);
                }
            }
            else
            {
                this->loadWorker.SetFileName3("");
            }
        }
    }
    return LV2_State_Status::LV2_STATE_SUCCESS;
}

// plugin creates the directories, not the host.

void ToobConvolutionReverb::MaybeCreateSampleDirectory(const std::filesystem::path &audioFileDirectory)
{
    // to be deletable, impulse files must be in a user-modifiable directory.
    // We will create an audioFileDirectory with soft links to the files in our bundle directory.
    // (permissioning doesn't allow hard links).
    // We'll also version check using a file in the audioFileDirectory.

    // check to see what version of sample files have been installed.
    uint32_t sampleVersion = 0;

    std::filesystem::create_directories(audioFileDirectory);

    std::string folder = IsConvolutionReverb() ? "reverb" : "CabIR";
    std::filesystem::path resourceDirectory = std::filesystem::path(this->getBundlePath()) / "impulseFiles" / folder;

    std::filesystem::path versionFilePath = audioFileDirectory / VERSION_FILENAME;
    try
    {
        std::ifstream f(versionFilePath);
        if (f.is_open())
        {
            f >> sampleVersion;
        }
    }
    catch (const std::exception &)
    {
    }

    if (sampleVersion >= SAMPLE_FILES_VERSION) // already created links? Don't do it again.
    {
        return;
    }

    std::filesystem::create_directories(audioFileDirectory);

    try
    {
        // hard-link any resource files into user-accessible directory.
        for (auto const &dir_entry : std::filesystem::directory_iterator(resourceDirectory))
        {
            const std::filesystem::path &resourceFilePath = dir_entry.path();

            std::filesystem::path targetFilePath = audioFileDirectory / resourceFilePath.filename();
            if (!std::filesystem::exists(targetFilePath))
            {
                std::filesystem::create_symlink(resourceFilePath, targetFilePath);
            }
        }
        std::ofstream f(versionFilePath, std::ios_base::trunc);
        if (f.is_open())
        {
            f << SAMPLE_FILES_VERSION << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        this->LogError("%s", SS("Can't create reverb impulse file directory. " << e.what()).c_str());
    }
}

void ToobConvolutionReverb::SetDefaultFile(const LV2_Feature *const *features)
{
    if (IsConvolutionReverb())
    {
        std::filesystem::path targetPath;
        LV2_State_Status res = GetUserResourcePath(features, &targetPath);
        if (res != LV2_State_Status::LV2_STATE_SUCCESS)
        {
            this->loadWorker.SetFileName("");
        }

        std::filesystem::path defaultFilePath = targetPath / "Genesis 6 Studio Live Room.wav";
        this->loadWorker.SetFileName(defaultFilePath.c_str());
    }
}
