/*
Copyright (c) 2022 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "NeuralAmpModeler.h"
#include <vector>
#include "lv2/core/lv2.h"
#include "lv2/atom/atom.h"
#include <iostream>
#include <chrono>

#ifdef WITHGPERFTOOLS
#include <gperftools/profiler.h>
#endif


using namespace toob;
using namespace std;

//// LV2 MAP FEATURE //////////////////////////////////////////
#include "lv2/core/lv2.h"
#include "lv2/log/logger.h"
#include "lv2/log/log.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"
#include "lv2/atom/atom.h"
#include <map>
#include <string>
#include <mutex>

namespace pipedal
{
    class MapFeature
    {

    private:
        LV2_URID nextAtom = 0;
        LV2_Feature mapFeature;
        LV2_Feature unmapFeature;
        LV2_URID_Map map;
        LV2_URID_Unmap unmap;
        std::map<std::string, LV2_URID> stdMap;
        std::map<LV2_URID, std::string *> stdUnmap;
        std::mutex mapMutex;

    public:
        MapFeature();
        ~MapFeature();

    public:
        const LV2_Feature *GetMapFeature()
        {
            return &mapFeature;
        }
        const LV2_Feature *GetUnmapFeature()
        {
            return &unmapFeature;
        }
        LV2_URID GetUrid(const char *uri);

        const char *UridToString(LV2_URID urid);

        const LV2_URID_Map *GetMap() const { return &map; }
        LV2_URID_Map *GetMap() { return &map; }
    };

    static LV2_URID mapFn(LV2_URID_Map_Handle handle, const char *uri)
    {
        MapFeature *feature = (MapFeature *)(void *)handle;
        return feature->GetUrid(uri);
    }
    static const char *unmapFn(LV2_URID_Map_Handle handle, LV2_URID urid)
    {
        MapFeature *feature = (MapFeature *)(void *)handle;
        return feature->UridToString(urid);
    }

    MapFeature::MapFeature()
    {
        mapFeature.URI = LV2_URID__map;
        mapFeature.data = &map;
        map.handle = (void *)this;
        map.map = &mapFn;

        unmapFeature.URI = LV2_URID__unmap;
        unmapFeature.data = &unmap;
        unmap.handle = (void *)this;
        unmap.unmap = &unmapFn;
    }

    MapFeature::~MapFeature()
    {
        for (auto i = stdUnmap.begin(); i != stdUnmap.end(); ++i)
        {
            delete i->second;
        }
    }

    LV2_URID MapFeature::GetUrid(const char *uri)
    {

        std::lock_guard<std::mutex> guard(mapMutex);

        LV2_URID result = stdMap[uri];
        if (result == 0)
        {
            stdMap[uri] = ++nextAtom;
            result = nextAtom;
            std::string *stringRef = new std::string(uri);
            stdUnmap[result] = stringRef;
        }
        return result;
    }

    const char *MapFeature::UridToString(LV2_URID urid)
    {
        std::lock_guard<std::mutex> guard(mapMutex);

        std::string *pResult = stdUnmap[urid];
        if (pResult == nullptr)
            return nullptr;

        return pResult->c_str();
    }

}
using namespace pipedal;

//////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{

    constexpr size_t FRAME_SIZE = 64;
    constexpr size_t SEQUENCE_SIZE = 16 * 1024;
    constexpr uint64_t SAMPLE_RATE = 44100;
    constexpr size_t TEST_SECONDS = 20;

    cout << "ProfileNeuralAmpModeler" << endl;
    cout << "Copyright (c) 2023 Robin E. R. Davies" << endl;
    cout << endl;
    if (argc != 2 || argv[1][0] == '-')
    {
        cout << "Syntax:  ProfileNeuralAmpModeler filename" << endl;
        cout << "         where filename is a path to a valid .nam model file." << endl;
        return EXIT_FAILURE;
    }


    MapFeature mapFeature;

    std::vector<const LV2_Feature*> features;
    features.push_back(mapFeature.GetMapFeature());
    features.push_back(mapFeature.GetUnmapFeature());
    features.push_back(nullptr);

    Lv2Plugin *plugin = NeuralAmpModeler::Create(SAMPLE_RATE, "", &(features[0]));
    NeuralAmpModeler *namModeler = static_cast<NeuralAmpModeler*>(plugin);

    vector<float> input(FRAME_SIZE);
    vector<float> output(FRAME_SIZE);

    vector<uint8_t> control_mem(SEQUENCE_SIZE);
    vector<uint8_t> notify_mem(SEQUENCE_SIZE);

    LV2_Atom_Sequence *controlInput = (LV2_Atom_Sequence *)(&control_mem[0]);
    LV2_Atom_Sequence *controlOutput = (LV2_Atom_Sequence *)(&notify_mem[0]);

    float inputLevel = 0, outputLevel = 0, gateThreshold = 0, gateOutput = 0;
    float bass = 0.1f, mid = 1.0f, treble = 0.1f;
    float toneStackType = 1;
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kInputGain, &inputLevel);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kOutputGain, &outputLevel);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kNoiseGateThreshold, &gateThreshold);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kGateOut, &gateOutput);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kBass, &bass);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kMid, &mid);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kTreble, &treble);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kStackType, &toneStackType);


    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kAudioIn, &(input[0]));
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kAudioOut, &(output[0]));
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kControlIn, controlInput);
    plugin->ConnectPort((int32_t)NeuralAmpModeler::EParams::kControlOut, controlOutput);


    if (!namModeler->LoadModel(argv[1]))
    {
        return EXIT_FAILURE;
    }
    plugin->Activate();

    LV2_URID atom__Sequence = mapFeature.GetUrid(LV2_ATOM__Sequence);
    LV2_URID atom__frameTime = mapFeature.GetUrid(LV2_ATOM__frameTime);


#ifdef  WITHGPERFTOOLS
    ProfilerStart("/tmp/ProfileNeuralAmpModeler.perf");
#endif
    auto start = std::chrono::high_resolution_clock::now();

    double x = 0;
    double dx = 440*3.14159*2/SAMPLE_RATE;
    for (size_t sample = 0; sample < SAMPLE_RATE * TEST_SECONDS; sample += FRAME_SIZE)
    {
        controlInput->atom.type = atom__Sequence;
        controlInput->atom.size = sizeof(LV2_Atom_Sequence::body);
        controlInput->body.unit = atom__frameTime;

        for (size_t i = 0; i < FRAME_SIZE; ++i)
        {
            input[i] = std::sin(x);
            x += dx;
        }

        controlOutput->atom.type = 0;
        controlOutput->atom.size = SEQUENCE_SIZE-sizeof(LV2_Atom);

        plugin->Run(FRAME_SIZE);
    }
    auto elapsed = std::chrono::high_resolution_clock::now()-start;
#ifdef  WITHGPERFTOOLS
    ProfilerStop();
#endif


    std::chrono::microseconds us = std::chrono::duration_cast<chrono::milliseconds>(elapsed);
    cout << "Ellapsed ms: " << (us.count()/1000.0) << endl;

    

    plugin->Deactivate();

    return EXIT_SUCCESS;
}