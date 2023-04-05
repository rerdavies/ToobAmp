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
#include "ss.hpp"
#include "LsNumerics/BalancedConvolution.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "ss.hpp"

#define TOOB_CONVOLUTION_REVERB_URI "http://two-play.com/plugins/toob-convolution-reverb"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

using namespace toob;

const float MAX_DELAY_MS = 4000;
const float NOMINAL_DELAY_MS = 1600;

ToobConvolutionReverb::ToobConvolutionReverb(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2PluginWithState(features),
      rate(rate),
      bundle_path(bundle_path),
      loadWorker(this)

{
    urids.Init(this);
    loadWorker.Initialize((size_t)rate, this);
    std::filesystem::path planFilePath{bundle_path};
    planFilePath = planFilePath / "fftplans";
    BalancedConvolutionSection::SetPlanFileDirectory(planFilePath.string());
}

const char *ToobConvolutionReverb::URI = TOOB_CONVOLUTION_REVERB_URI;

void ToobConvolutionReverb::ConnectPort(uint32_t port, void *data)
{
    switch ((PortId)port)
    {
    case PortId::TIME:
        this->pTime = (float *)data;
        break;
    case PortId::DIRECT_MIX:
        this->pDirectMix = (float *)data;
        break;
    case PortId::REVERB_MIX:
        this->pReverbMix = (float *)data;
        break;
    case PortId::LOADING_STATE:
        break;

    case PortId::AUDIO_INL:
        this->inL = (const float *)data;
        break;
    case PortId::AUDIO_OUTL:
        this->outL = (float *)data;
        break;
    case PortId::CONTROL_IN:
        this->controlIn = (LV2_Atom_Sequence *)data;
        break;
    case PortId::CONTROL_OUT:
        this->controlOut = (LV2_Atom_Sequence *)data;
        break;
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
        if (lastDirectMix <= -30)
        {
            directMix = 0;
        }
        else
        {
            directMix = db2a(lastDirectMix);
        }
        if (pConvolutionReverb)
        {
            pConvolutionReverb->SetDirectMix(directMix);
        }
    }
    if (lastReverbMix != *pReverbMix)
    {
        lastReverbMix = *pReverbMix;
        if (lastReverbMix <= -60)
        {
            reverbMix = 0;
        }
        else
        {
            reverbMix = db2a(lastReverbMix);
        }
        if (pConvolutionReverb)
        {
            pConvolutionReverb->SetReverbMix(reverbMix);
        }
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
        loadWorker.Tick();
        if (pConvolutionReverb)
        {
            pConvolutionReverb->Tick(n_samples, inL, outL);
        }
        else
        {
            for (uint32_t i = 0; i < n_samples; ++i)
            {
                this->outL[i] = this->inL[i];
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

void ToobConvolutionReverb::OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *atom)
{
    if (propertyUrid == urids.propertyFileName && atom->type == urids.atom_path)
    {
        // LV2 declaration is insufficient to locate the body.
        typedef struct
        {
            LV2_Atom atom;   /**< Atom header. */
            const char c[1]; /* Contents (a null-terminated UTF-8 string) follow here. */
        } LV2_Atom_String_x;

        const char *name = ((LV2_Atom_String_x *)(atom))->c;
        bool changed = loadWorker.SetFileName(name);
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
    if (propertyUrid == urids.propertyFileName)
    {
        PutPatchPropertyPath(urids.propertyFileName, urids.propertyFileName, this->loadWorker.GetFileName());
    }
}

ToobConvolutionReverb::LoadWorker::LoadWorker(Lv2Plugin *pPlugin)
    : base(pPlugin)
{
    pThis = dynamic_cast<ToobConvolutionReverb*>(pPlugin);
    memset(fileName, 0, sizeof(fileName));
    memset(requestFileName, 0, sizeof(requestFileName));
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
    SetState(State::SentRequest);


    // take the existing convolution reverb off the main thread.
    this->oldConvolutionReverb = std::move(pReverb->pConvolutionReverb);

    WorkerAction::Request();
}

static void NormalizeConvolution(AudioData & data)
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
        std::cout << "MaxValue: " << maxValue << std::endl;

        float  scale = (float)(1/maxValue);

        for (size_t i = 0; i < size; ++i)
        {
            channel[i] *= scale;
        }
    }
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
void ToobConvolutionReverb::LoadWorker::OnWork()
{
    // non-audio thread. Memory allocations are allowed!

    this->oldConvolutionReverb = nullptr; // destroy the old convolution reverb if it exists.

    pThis->LogNote("%s",SS("Loading " << requestFileName).c_str());
    hasWorkError = false;
    workError = "";
    try
    {
        WavReader reader;

        reader.Open(requestFileName);
        AudioData data;
        reader.Read(data);

            pThis->LogNote(SS("File loaded. Sample rate: " << data.getSampleRate() << std::setw(4) << " Length: " << (data.getSize()*1.0f/data.getSampleRate()) << " seconds.").c_str());

        // Assume files with 4 channels are in Ambisonic b-Format.
        if (data.getChannelCount() == 4)
        {
            data.AmbisonicDownmix({ AmbisonicMicrophone(0,0)});
        } else {
            data.ConvertToMono();
        }

        NormalizeConvolution(data);

        data.Resample((size_t)pReverb->getRate());

        NormalizeConvolution(data);


        size_t maxSize = (size_t)std::ceil(pReverb->getTime() * pReverb->getRate());
        this->tailScale = 0;
        if (maxSize < data.getSize())
        {
            this->tailScale = GetTailScale(data.getChannel(0),maxSize);
            pThis->LogNote(SS("Feedback: " << tailScale).c_str());
            data.setSize(maxSize);


        }
        pThis->LogNote(SS("Sample rate: " << data.getSampleRate() << " Length: " << std::setw(4) << (data.getSize()*1.0f/data.getSampleRate()) << " seconds.").c_str());
        pThis->LogNote("Building convolution.");

        this->convolutionReverbResult = std::make_shared<ConvolutionReverb>(SchedulerPolicy::Realtime, data.getSize(), data.getChannel(0));
        if (tailScale != 0)
        {
        }
        this->convolutionReverbResult->SetFeedback(tailScale,data.getSize()-1);
        pThis->LogNote("Complete.");
    }
    catch (const std::exception &e)
    {
        hasWorkError = true;
        workError = SS("Can't load file " << requestFileName << ". (" << e.what() << ")");
        workError.c_str(); // allocate zero if neccessary.
    }
}
void ToobConvolutionReverb::LoadWorker::OnResponse()
{
    if (hasWorkError)
    {
        pReverb->LogError(workError.c_str());
        SetState(State::Error);
    }
    else
    {
        convolutionReverbResult->SetDirectMix(pReverb->directMix);
        convolutionReverbResult->SetReverbMix(pReverb->reverbMix);
        pReverb->pConvolutionReverb = std::move(convolutionReverbResult);
        // pConvolutionReverb now contains the old convolution, which we must dispose of
        // off the audio thread.
        SetState(State::CleaningUp);
    }
}

void ToobConvolutionReverb::LoadWorker::OnCleanup()
{
    this->convolutionReverbResult = nullptr;
}
void ToobConvolutionReverb::LoadWorker::OnCleanupComplete()
{
    SetState(State::Idle);
}

LV2_State_Status
ToobConvolutionReverb::OnSaveLv2State(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    auto status =store(handle,
          urids.propertyFileName,
          this->loadWorker.GetFileName(),
          strlen(this->loadWorker.GetFileName()) + 1,
          urids.atom_path,
          LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    if (status != LV2_State_Status::LV2_STATE_SUCCESS)
    {
        return status;
    }
    return LV2_State_Status::LV2_STATE_SUCCESS;
}

static void*GetFeature(const LV2_Feature *const *features,const char*featureUri)
{
    while (*features != nullptr)
    {
        if (strcmp((*features)->URI,featureUri) == 0)
        {
            return (*features)->data;
        }
        ++features;
    }
    return nullptr;
}

// State extension callbacks.
LV2_State_Status
ToobConvolutionReverb::OnRestoreLv2State(
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    size_t      size;
    uint32_t    type;
    uint32_t    myFlags;
    const void* data = retrieve(
        handle, urids.propertyFileName, &size, &type, &myFlags);

    const LV2_State_Map_Path*mapPath =  (const LV2_State_Map_Path*)GetFeature(features,LV2_STATE__mapPath);
    const LV2_State_Make_Path*makePath =  (const LV2_State_Make_Path*)GetFeature(features,LV2_STATE__makePath);


    if (makePath == nullptr)
    {
        return LV2_State_Status::LV2_STATE_ERR_NO_FEATURE;
    }

    // Use makePath to get a user-modifiable directory.
    std::filesystem::path targetPath = std::filesystem::path("ReverbImpulseFiles");
    {
        const char*mappedPath = makePath->path(makePath->handle,targetPath.c_str());
        targetPath = mappedPath;
        free((void*)mappedPath);
    }
    // create sample files in user-modifiable diretory if required.
    this->MaybeCreateSampleDirectory(targetPath);

    if (data)
    {
        if (type != this->urids.atom_path)
        {
            return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
        }
        if (mapPath == nullptr)
        {
            return LV2_State_Status::LV2_STATE_ERR_NO_FEATURE;
        }
        const char*absolutePath =  mapPath->absolute_path(makePath->handle,(const char*)data);
        this->loadWorker.SetFileName((const char*)data);
        free((void*)absolutePath);
    } else {

        std::filesystem::path defaultFilePath = targetPath / "Genesis 6 Studio Live Room.wav";
        this->loadWorker.SetFileName(defaultFilePath.c_str());
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

    std::filesystem::path resourceDirectory = std::filesystem::path(this->getBundlePath()) / "impulseFiles" / "reverb";

    std::filesystem::path versionFilePath = audioFileDirectory / VERSION_FILENAME;
    try {
        std::ifstream f(versionFilePath);
        if (f.is_open())
        {
            f >> sampleVersion;
        }
    } catch (const std::exception&)
    {
    }

    if (sampleVersion >= SAMPLE_FILES_VERSION) // already created links? Don't do it again.
    {
        return;
    }


    std::filesystem::create_directories(audioFileDirectory);

    try {
        // hard-link any resource files into user-accessible directory.
        for (auto const &dir_entry : std::filesystem::directory_iterator(resourceDirectory))
        {
            const std::filesystem::path &resourceFilePath = dir_entry.path();

            std::filesystem::path targetFilePath  = audioFileDirectory / resourceFilePath.filename();
            if (!std::filesystem::exists(targetFilePath))
            {
                std::filesystem::create_symlink(resourceFilePath,targetFilePath);
            }
        }
        std::ofstream f(versionFilePath,std::ios_base::trunc);
        if (f.is_open())
        {
            f << SAMPLE_FILES_VERSION << std::endl;
        }
    } catch (const std::exception &e)
    {
        this->LogError("%s",SS("Can't create reverb impulse file directory. " << e.what()).c_str());
    }

}
