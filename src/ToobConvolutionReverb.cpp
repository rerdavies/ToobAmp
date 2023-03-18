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

#define TOOB_CONVOLUTION_REVERB_URI "http://two-play.com/plugins/toob-convolution-reverb"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

using namespace TwoPlay;

const float MAX_DELAY_MS = 4000;
const float NOMINAL_DELAY_MS = 1600;

ToobConvolutionReverb::ToobConvolutionReverb(
    double rate,
    const char *bundle_path,
    const LV2_Feature *const *features)
    : Lv2Plugin(features),
      rate(rate),
      bundle_path(bundle_path),
      loadWorker(this)

{
    urids.Init(this);
    loadWorker.Initialize((size_t)rate, this);
    std::filesystem::path planFilePath {bundle_path};
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
        this->controlIn = (LV2_Atom_Sequence*)data;
        break;
    case PortId::CONTROL_OUT:
        this->controlOut = (LV2_Atom_Sequence*)data;
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

void ToobConvolutionReverb::UpdateConvolution()
{
    if (!activated)
        return;
    // XXX
    // pConvolutionReverb
}
inline void ToobConvolutionReverb::updateControls()
{
    if (lastTime != *pTime)
    {
        lastTime = *pTime;
        time = lastTime;
        UpdateConvolution();
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
        if (lastReverbMix <= -30)
        {
            reverbMix = 0;
        }
        else
        {
            reverbMix = db2a(lastReverbMix);
        }
        if (pConvolutionReverb)
        {
            pConvolutionReverb->SetReverbMix(directMix);
        }
    }
}
void ToobConvolutionReverb::Activate()
{
    activated = true;
    lastReverbMix = lastDirectMix = lastTime = std::numeric_limits<float>::min(); // force updates
    updateControls();
    clear();
    loadWorker.SetFileName("/home/pi/src/ToobAmp/impulseFiles/reverb/Studio Nord Foil 1,5sec flat.wav");
}

void ToobConvolutionReverb::Run(uint32_t n_samples)
{
    SetAtomOutput(this->controlOut);
    HandleEvents(this->controlIn);
    updateControls();
    loadWorker.Tick();
    if (pConvolutionReverb)
    {
        pConvolutionReverb->Tick(n_samples, inL, outL);
    } else {
        for (uint32_t i = 0; i < n_samples; ++i)
        {
            this->outL[i] = this->inL[i];
        }
    }
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
        loadWorker.SetFileName(name);
    }
}

void ToobConvolutionReverb::OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object *object)
{
    if (propertyUrid == urids.propertyFileName)
    {
        PatchPutString(urids.propertyFileName,urids.atom_path,this->loadWorker.GetFileName());
    }
}

ToobConvolutionReverb::LoadWorker::LoadWorker(Lv2Plugin *pPlugin)
    : base(pPlugin)
{
    memset(fileName, 0, sizeof(fileName));
    memset(requestFileName,0,sizeof(requestFileName));
}

void ToobConvolutionReverb::LoadWorker::Initialize(size_t sampleRate, ToobConvolutionReverb *pReverb)
{
    this->sampleRate = sampleRate;
    this->pReverb = pReverb;
}
void ToobConvolutionReverb::LoadWorker::SetFileName(const char *szName)
{
    size_t length = strlen(szName);
    if (length >= sizeof(fileName) - 1)
    {
        pReverb->LogError("File name too long.");
        SetState(State::Error);
        return; 
    }

    bool changed = strncmp(fileName,szName,sizeof(fileName)-1) != 0;
    if (!changed)
        return;
    this->changed = true;
    strncpy(fileName, szName, sizeof(fileName)-1);
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
    WorkerAction::Request();
}
void ToobConvolutionReverb::LoadWorker::OnWork()
{
    // non-audio thread. Memory allocations are allowed.
    hasWorkError = false;
    workError = "";
    try
    {
        WavReader reader;

        reader.Open(requestFileName);
        AudioData data;
        reader.Read(data);
        data.ConvertToMono();
        data.Resample((size_t)pReverb->getRate());

        size_t maxSize = (size_t)std::ceil(pReverb->getTime()*pReverb->getRate());
        if (maxSize > data.getSize())
        {
            data.setSize(maxSize);
        }
        this->convolutionReverbResult = std::make_shared<ConvolutionReverb>(data.getSize(),data.getChannel(0));
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
    } else {
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

void Lv2Plugin::SetAtomOutput(LV2_Atom_Sequence*controlOutput)
{
    const uint32_t notify_capacity = controlOutput->atom.size;
    lv2_atom_forge_set_buffer(
        &(this->outputForge), (uint8_t*)(controlOutput), notify_capacity);
}
