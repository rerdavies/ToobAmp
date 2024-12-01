/*
 *   Copyright (c) 2023 Robin E. R. Davies
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
#include "lv2ext/filedialog.h"

#include "db.h"
#include "ss.hpp"
#include <thread>
#include "WavReader.hpp"
#include "FlacReader.hpp"
#include "ss.hpp"
#include "LsNumerics/ConvolutionReverb.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "ss.hpp"

#define TOOB_CONVOLUTION_REVERB_URI "http://two-play.com/plugins/toob-convolution-reverb"
#define TOOB_CONVOLUTION_REVERB_STEREO_URI "http://two-play.com/plugins/toob-convolution-reverb-stereo"
#define TOOB_CAB_IR_URI "http://two-play.com/plugins/toob-cab-ir"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

using namespace toob;

constexpr float MIN_MIX_DB = -40;

ToobConvolutionReverb::ToobConvolutionReverb(
    PluginType pluginType,
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2PluginWithState(bundle_path, features),
      sampleRate(rate),
      bundle_path(bundle_path),
      loadWorker(this),
      isConvolutionReverb(pluginType != PluginType::CabIr),
      pluginType(pluginType),
      isStereo(pluginType == PluginType::ConvolutionReverbStereo)

{
    urids.Init(this);
    loadWorker.Initialize((size_t)rate, this);

    SetDefaultFile(features);

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
const char *ToobConvolutionReverb::CONVOLUTION_REVERB_STEREO_URI = TOOB_CONVOLUTION_REVERB_STEREO_URI;
const char *ToobConvolutionReverb::CAB_IR_URI = TOOB_CAB_IR_URI;

void ToobConvolutionReverb::ConnectPort(uint32_t port, void *data)
{
    switch (pluginType)
    {
    case PluginType::ConvolutionReverb:
    {
        switch ((MonoReverbPortId)port)
        {
        case MonoReverbPortId::TIME:
            this->pTime = (float *)data;
            break;
        case MonoReverbPortId::DIRECT_MIX:
            this->pDirectMix = (float *)data;
            break;
        case MonoReverbPortId::REVERB_MIX:
            this->pReverbMix = (float *)data;
            break;
        case MonoReverbPortId::PREDELAY:
            this->pPredelay = (float *)data;
            break;
        case MonoReverbPortId::LOADING_STATE:
            this->pLoadingState = (float *)data;
            if (this->pLoadingState)
            {
                *(this->pLoadingState) = this->loadingState;
            }
            break;
        case MonoReverbPortId::AUDIO_INL:
            this->inL = (const float *)data;
            break;
        case MonoReverbPortId::AUDIO_OUTL:
            this->outL = (float *)data;
            break;
        case MonoReverbPortId::CONTROL_IN:
            this->controlIn = (LV2_Atom_Sequence *)data;
            break;
        case MonoReverbPortId::CONTROL_OUT:
            this->controlOut = (LV2_Atom_Sequence *)data;
            break;
        default:
            this->LogError("%s\n", SS("Illegal port id: " << port).c_str());
            break;
        }
    }
    break;
    case PluginType::ConvolutionReverbStereo:
    {
        switch ((StereoReverbPortId)port)
        {
        case StereoReverbPortId::TIME:
            this->pTime = (float *)data;
            break;
        case StereoReverbPortId::DIRECT_MIX:
            this->pDirectMix = (float *)data;
            break;
        case StereoReverbPortId::REVERB_MIX:
            this->pReverbMix = (float *)data;
            break;
        case StereoReverbPortId::PREDELAY:
            this->pPredelay = (float *)data;
            break;
        case StereoReverbPortId::LOADING_STATE:
            this->pLoadingState = (float *)data;
            if (this->pLoadingState)
            {
                *(this->pLoadingState) = this->loadingState;
            }
            break;
        case StereoReverbPortId::WIDTH:
            this->pWidth = (float *)data;
            break;
        case StereoReverbPortId::PAN:
            this->pPan = (float *)data;
            break;

        case StereoReverbPortId::AUDIO_INL:
            this->inL = (const float *)data;
            break;
        case StereoReverbPortId::AUDIO_OUTL:
            this->outL = (float *)data;
            break;
        case StereoReverbPortId::AUDIO_INR:
            this->inR = (const float *)data;
            break;
        case StereoReverbPortId::AUDIO_OUTR:
            this->outR = (float *)data;
            break;
        case StereoReverbPortId::CONTROL_IN:
            this->controlIn = (LV2_Atom_Sequence *)data;
            break;
        case StereoReverbPortId::CONTROL_OUT:
            this->controlOut = (LV2_Atom_Sequence *)data;
            break;
        default:
            this->LogError("%s\n", SS("Illegal port id: " << port).c_str());
            break;
        }
    }
    break;

    case PluginType::CabIr:
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
        default:
            this->LogError("%s\n", SS("Illegal port id: " << port).c_str());
        }
        break;
    default:
        throw std::logic_error(SS("Invalid plugin type."));
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
    if (pPan != nullptr && lastPan != *pPan)
    {
        lastPan = *pPan;
        loadWorker.SetPan(lastPan);
    }
    if (pWidth != nullptr && lastWidth != *pWidth)
    {
        lastWidth = *pWidth;
        loadWorker.SetWidth(lastWidth);
    }
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
                if (pConvolutionReverb)
                {
                    pConvolutionReverb->SetReverbMix(reverbMixAf);
                }
            }
        }
        else
        {
            loadWorker.SetMix(reverbMixAf);
        }
    }
    if (pReverb2Mix != nullptr && lastReverb2Mix != *pReverb2Mix)
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
    if (pReverb3Mix != nullptr && lastReverb3Mix != *pReverb3Mix)
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
    RequestNotifyOnLoad();
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
            if (isStereo)
            {
                pConvolutionReverb->Tick(n_samples, inL, inR, outL, outR);
            }
            else
            {
                pConvolutionReverb->Tick(n_samples, inL, outL);
            }
        }
        else
        {
            for (uint32_t i = 0; i < n_samples; ++i)
            {
                this->outL[i] = 0;
            }
            if (this->outR)
            {
                for (uint32_t i = 0; i < n_samples; ++i)
                {
                    this->outR[i] = 0;
                }
            }
        }
        NotifyProperties();
    }

    // absolutely ignore hosts that set *pLoadingState.
    *(pLoadingState) = this->loadingState;
}

void ToobConvolutionReverb::CancelLoad()
{
    // there's nothing worthwhile that we can do to cancel the load.
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
            this->stateChanged = true;
            if (propertyUrid == urids.reverb__propertyFileName)
            {
                notifyReverbFileName = true;
            }
            else
            {
                notifyCabIrFileName = true;
            }
        }
    }
    if (propertyUrid == urids.cabir__propertyFileName2)
    {
        std::string name = StringFromAtomPath(atom);

        bool changed = loadWorker.SetFileName2(name.c_str());
        if (changed)
        {
            this->stateChanged = true;
            notifyCabIrFileName2 = true;
        }
    }
    if (propertyUrid == urids.cabir__propertyFileName3)
    {
        std::string name = StringFromAtomPath(atom);

        bool changed = loadWorker.SetFileName3(name.c_str());
        if (changed)
        {
            this->stateChanged = true;
            notifyCabIrFileName3 = true;
        }
    }
}

void ToobConvolutionReverb::OnPatchGetAll()
{
    // PutPatchPropertyPath(urids.propertyFileName, urids.propertyFileName, this->loadWorker.GetFileName());
}

void ToobConvolutionReverb::RequestNotifyOnLoad()
{
    // Carla and MOD require property notification after first load, or after a restore.
    if (IsConvolutionReverb())
    {
        notifyReverbFileName = true;
    }
    else
    {
        notifyCabIrFileName = notifyCabIrFileName2 = notifyCabIrFileName3 = true;
    }
}
void ToobConvolutionReverb::OnPatchGet(LV2_URID propertyUrid)
{

    if (propertyUrid == urids.reverb__propertyFileName)
    {
        notifyReverbFileName = true;
    }
    else if (propertyUrid == urids.cabir__propertyFileName)
    {
        notifyCabIrFileName = true;
    }
    else if (propertyUrid == urids.cabir__propertyFileName2)
    {
        notifyCabIrFileName2 = true;
    }
    else if (propertyUrid == urids.cabir__propertyFileName3)
    {
        notifyCabIrFileName3 = true;
    }
}
void ToobConvolutionReverb::NotifyProperties()
{
    if (this->stateChanged)
    {
        this->stateChanged = false;
        PutStateChanged(0);
    }

    if (notifyReverbFileName)
    {
        notifyReverbFileName = false;
        PutPatchPropertyPath(0, urids.reverb__propertyFileName, this->loadWorker.GetFileName());
    }
    if (notifyCabIrFileName)
    {
        notifyCabIrFileName = false;
        PutPatchPropertyPath(0, urids.cabir__propertyFileName, this->loadWorker.GetFileName());
    }
    if (notifyCabIrFileName2)
    {
        notifyCabIrFileName2 = false;

        PutPatchPropertyPath(0, urids.cabir__propertyFileName2, this->loadWorker.GetFileName2());
    }
    if (notifyCabIrFileName3)
    {
        notifyCabIrFileName3 = false;
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
    size_t bufferSize = pReverb->GetBuffSizeOptions().nominalBlockLength;
    if (bufferSize == 0 || bufferSize == (size_t)-1)
        bufferSize = 256;
    if (bufferSize > 1024)
        bufferSize = 1024;

    this->audioBufferSize = bufferSize;
    this->pReverb = pReverb;
}

bool ToobConvolutionReverb::LoadWorker::SetWidth(float width)
{
    if (this->width != width)
    {
        this->width = width;
        this->changed = true;
        return true;
    }
    return false;
}
bool ToobConvolutionReverb::LoadWorker::SetPan(float pan)
{
    if (this->pan != pan)
    {
        this->pan = pan;
        this->changed = true;
        return true;
    }
    return false;
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
        pReverb->LogError("File name too long.\n");
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
        pReverb->LogError("File name too long.\n");
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
        pReverb->LogError("File name too long.\n");
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
    this->requestWidth = this->width;
    this->requestMix = this->mix;
    this->requestPan = this->pan;
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
    if (pThis->isStereo)
    {
        switch (data.getChannelCount())
        {
        case 1:
            data.MonoToStereo();
            break;
        case 2:
        default:
            data.SetStereoWidth(this->requestWidth);
            break;
        case 4:
        {
            float angle = 90 * (this->requestWidth);
            data.AmbisonicDownmix({AmbisonicMicrophone(-angle + 90 * requestPan, 0), AmbisonicMicrophone(angle + 90 * requestPan, 0)});
        }
        break;
        }
    }
    else
    {
        if (data.getChannelCount() == 4)
        {
            data.AmbisonicDownmix({AmbisonicMicrophone(0, 0)});
        }
        else
        {
            data.ConvertToMono();
        }
    }
    pThis->LogTrace("%s\n", SS("File loaded. Sample rate: " << data.getSampleRate() << std::setprecision(3) << " Length: " << (data.getSize() * 1.0f / data.getSampleRate()) << "s.").c_str());

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

    pThis->LogTrace("%s\n", SS("Loading " << requestFileName).c_str());
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

            pThis->LogTrace("%s\n", SS("Max T: " << std::setprecision(3) << workingTimeInSeconds << "s Feedback: " << tailScale).c_str());
        }
        if (data.getSize() == 0)
        {
            data.setSize(1);
        }
        if (pThis->isStereo)
        {
            this->convolutionReverbResult = std::make_shared<ConvolutionReverb>(
                SchedulerPolicy::Realtime,
                data.getSize(), data.getChannel(0), data.getChannel(1),
                sampleRate,
                audioBufferSize);
        }
        else
        {
            this->convolutionReverbResult = std::make_shared<ConvolutionReverb>(SchedulerPolicy::Realtime,
                                                                                data.getSize(), data.getChannel(0),
                                                                                sampleRate,
                                                                                audioBufferSize);
        }
        this->convolutionReverbResult->SetFeedback(tailScale, data.getSize() - 1);
        pThis->LogTrace("Load complete.\n");
    }
    catch (const std::exception &e)
    {
        hasWorkError = true;
        workError = e.what();
    }
}
void ToobConvolutionReverb::LoadWorker::OnResponse()
{
    if (hasWorkError)
    {
        pReverb->LogError("%s\n", workError.c_str());
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

std::string ToobConvolutionReverb::UnmapFilename(const LV2_Feature *const *features, const std::string &fileName)
{
    // const LV2_State_Make_Path *makePath = GetFeature<LV2_State_Make_Path>(features, LV2_STATE__makePath);
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath && fileName.length() != 0)
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

void ToobConvolutionReverb::SaveLv2Filename(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    const LV2_Feature *const *features,
    LV2_URID urid,
    const std::string &filename_)
{
    std::string fileName = UnmapFilename(features, filename_);
    auto status = store(handle,
                        urid,
                        fileName.c_str(),
                        fileName.length() + 1,
                        fileName.length() == 0 ? urids.atom__string : urids.atom__path,
                        LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    if (status != LV2_State_Status::LV2_STATE_SUCCESS)
    {
        LogError(SS("State property save failed. (" << status << ")"));
        return;
    }
}

LV2_State_Status
ToobConvolutionReverb::OnSaveLv2State(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    if (IsConvolutionReverb())
    {
        SaveLv2Filename(
            store, handle, features,
            urids.reverb__propertyFileName,
            loadWorker.GetFileName());
    }
    else
    {
        {
            SaveLv2Filename(
                store, handle, features,
                urids.cabir__propertyFileName,
                loadWorker.GetFileName());
        }
        {
            SaveLv2Filename(
                store, handle, features,
                urids.cabir__propertyFileName2,
                this->loadWorker.GetFileName2());
        }
        {
            SaveLv2Filename(
                store, handle, features,
                urids.cabir__propertyFileName3,
                this->loadWorker.GetFileName3());
        }
    }
    return LV2_State_Status::LV2_STATE_SUCCESS;
}

void ToobConvolutionReverb::PublishResourceFiles(
    const LV2_Feature *const *features)
{
    const LV2_FileBrowser_Files *fileBrowserFiles = GetFeature<LV2_FileBrowser_Files>(features, LV2_FILEBROWSER__files);

    if (fileBrowserFiles == nullptr)
    {
        return;
    }
    LV2_FileBrowser_Status status;
    if (IsConvolutionReverb())
    {
        constexpr int RESOURCE_VERSION = 1;
        status = fileBrowserFiles->publish_resource_files(fileBrowserFiles->handle, RESOURCE_VERSION, "impulseFiles/reverb", "ReverbImpulseFiles");
    }
    else
    {
        constexpr int RESOURCE_VERSION = 1;
        status = fileBrowserFiles->publish_resource_files(fileBrowserFiles->handle, RESOURCE_VERSION, "impulseFiles/CabIR", "CabIR");
    }
    if (status == LV2_FileBrowser_Status::LV2_FileBrowser_Status_Err_Filesystem)
    {
        LogWarning("%s: %s\n",
                IsConvolutionReverb() ? "TooB Convolution Reverb" : "Toob Cab IR",
                "Failed to publish resource audio files.");
    }
}

std::string ToobConvolutionReverb::MapFilename(
    const LV2_Feature *const *features,
    const std::string &input)
{

    if (input.starts_with(this->getBundlePath().c_str()))
    {
        // check for PiPedal extension that map bundle files into browser dialog directories.
        const LV2_FileBrowser_Files *browserFiles = GetFeature<LV2_FileBrowser_Files>(features, LV2_FILEBROWSER__files);
        if (browserFiles != nullptr)
        {
            char *t = nullptr;
            if (IsConvolutionReverb())
            {
                t = browserFiles->map_path(browserFiles->handle, input.c_str(), "impulseFiles/reverb", "ReverbImpulseFiles");
            }
            else
            {
                t = browserFiles->map_path(browserFiles->handle, input.c_str(), "impulseFiles/CabIR", "CabIR");
            }
            std::string result = t;
            browserFiles->free_path(browserFiles->handle, t);
            return result;
        }
        return input;
    }
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath == nullptr || input.length() == 0)
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

    RequestNotifyOnLoad();

    PublishResourceFiles(features);

    if (IsConvolutionReverb())
    {
        const void *data = retrieve(
            handle, urids.reverb__propertyFileName, &size, &type, &myFlags);
        if (data)
        {
            if (type != this->urids.atom__path && type != this->urids.atom__string)
            {
                return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
            }
            std::string input((const char *)data);
            this->loadWorker.SetFileName(MapFilename(features, input).c_str());
        }
        else
        {
            SetDefaultFile(features);
        }
    }
    else
    {
        {
            const void *data = retrieve(
                handle, urids.cabir__propertyFileName, &size, &type, &myFlags);
            if (data)
            {
                if (type != this->urids.atom__path && type != this->urids.atom__string)
                {
                    return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
                }
                std::string input((const char *)data);
                this->loadWorker.SetFileName(MapFilename(features, input).c_str());
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
                if (type != this->urids.atom__path && type != this->urids.atom__string)
                {
                    return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
                }
                std::string input((const char *)data);
                this->loadWorker.SetFileName2(MapFilename(features, input).c_str());
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
                if (type != this->urids.atom__path && type != this->urids.atom__string)
                {
                    return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
                }
                std::string input((const char *)data);
                this->loadWorker.SetFileName3(MapFilename(features, input).c_str());
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

void ToobConvolutionReverb::SetDefaultFile(const LV2_Feature *const *features)
{
    if (IsConvolutionReverb())
    {
        auto targetPath = std::filesystem::path(this->getBundlePath()) / "impulseFiles" / "reverb" / "Genesis 6 Studio Live Room.wav";
        this->loadWorker.SetFileName(targetPath.c_str());
    }
}
