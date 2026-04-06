// Copyright (c) 2026 Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#define DEFINE_LV2_PLUGIN_BASE

#include <chrono>
#include <filesystem>
#include <memory>
#include "ToobToneStereoInfo.hpp"
#include "ControlDezipper.h"
#include "Filters/ShelvingFilter.h"
#include "FilterResponse.h"
#include "DbDezipper.h"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

using namespace lv2c::lv2_plugin;
using namespace tone_plugin;
using namespace toob;

uint64_t timeMs();

class ToobTone : public tone_plugin::ToobToneStereoBase
{
public:
    using super = tone_plugin::ToobToneStereoBase;

    static Lv2Plugin *Create(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    {
        return new ToobTone(rate, bundle_path, features);
    }
    ToobTone(double rate,
             const char *bundle_path,
             const LV2_Feature *const *features);

    virtual ~ToobTone();

    static constexpr const char *URI = "http://two-play.com/plugins/toob-tone";
    static constexpr const char *STEREO_URI = "http://two-play.com/plugins/toob-tone-stereo";

protected:
    virtual void OnPatchGet(LV2_URID propertyUrid) override;

    virtual void Run(uint32_t n_samples) override;

    virtual void Activate() override;
    virtual void Deactivate() override;

private:
    bool isStereo = false;
    struct Uris
    {
    public:
        void Map(Lv2Plugin *plugin)
        {
            param_frequencyResponseVector = plugin->MapURI(TOOB_URI "#frequencyResponseVector");
            patch__Get = plugin->MapURI(LV2_PATCH__Get);
            patch__Set = plugin->MapURI(LV2_PATCH__Set);
            patch__property = plugin->MapURI(LV2_PATCH__property);
            patch__accept = plugin->MapURI(LV2_PATCH__accept);
            patch__value = plugin->MapURI(LV2_PATCH__value);
            atom__URID = plugin->MapURI(LV2_ATOM__URID);
            atom__Float = plugin->MapURI(LV2_ATOM__Float);
            atom__Int = plugin->MapURI(LV2_ATOM__Int);
            atom__String = plugin->MapURI(LV2_ATOM__String);
            atom__Path = plugin->MapURI(LV2_ATOM__Path);
            units__frame = plugin->MapURI(LV2_UNITS__frame);
        }
        LV2_URID param_frequencyResponseVector;

        LV2_URID patch;
        LV2_URID patch__Get;
        LV2_URID patch__Set;
        LV2_URID patch__property;
        LV2_URID patch__accept;
        LV2_URID patch__value;
        LV2_URID atom__URID;
        LV2_URID atom__Float;
        LV2_URID atom__Int;
        LV2_URID atom__String;
        LV2_URID atom__Path;
        LV2_URID units__frame;
    };

    Uris uris;

    LV2_Atom_Forge       forge;        ///< Forge for writing atoms in run thread


    FilterResponse filterResponse;

    void UpdateFilter();
    ShelvingFilter shelvingFilter;
    ShelvingFilter shelvingFilterR;
    float filterGain = 1.0;
    float filterGainDb = 0;

    void UpdateGain();
    DbDezipper gainDezipper;


    uint64_t frameTime = 0;
    float _rate = 44100;

    bool patchGet = false;
    bool responseChanged = true;
    int64_t updateSampleDelay;
    uint64_t updateMsDelay;

    int64_t updateSamples = 0;
    uint64_t updateMs = 0;


    void WriteFrequencyResponse();
    float CalculateFrequencyResponse(float f);



};
