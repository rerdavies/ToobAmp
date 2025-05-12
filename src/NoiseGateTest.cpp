// Copyright (c) 2025 Robin Davies
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



#include <Lv2Host.h>
#include "HostedLv2Plugin.h"
#include "ToobNoiseGateInfo.hpp"
#include <cassert>
#include "lv2_plugin/Lv2Ports.hpp"

using namespace toob;

using namespace lv2c::ui;
using namespace lv2c::lv2_plugin;

using PluginInfo = noise_gate_plugin::ToobNoiseGateUiBase;

template <typename T> 
class TestLv2Host: public Lv2Host {
public:
    TestLv2Host(const char*uri,double sampleRate = 48000, size_t maxBufferSize = 1024) 
    : Lv2Host(sampleRate,maxBufferSize)
    {
        this->plugin = CreatePlugin("build/src/ToobAmp.so",uri);
        controlData.resize(info().ports().size());

        for (int nPort = 0; nPort < (int)info().ports().size(); ++nPort)
        {
            const Lv2PortInfo &portInfo = info().ports()[nPort];
            if (portInfo.is_control_port()) 
            {
                int ix = portInfo.index();
                symbolToPortNumber[portInfo.symbol()] = nPort;
                plugin->ConnectPort(nPort,(void*)&(controlData[portInfo.index()]));
                controlData[ix] = portInfo.default_value();
            } else if (portInfo.is_audio_port()) {
                if (portInfo.is_input())
                {
                    size_t ix = audioInputs.size();
                    audioInputs.resize(ix+1);
                    audioInputs[ix].resize(maxBufferSize);
                    plugin->ConnectPort(portInfo.index(),(void*)(audioInputs[ix].data()));
                } else {
                    size_t ix = audioOutputs.size();
                    audioOutputs.resize(ix+1);
                    audioOutputs[ix].resize(maxBufferSize);
                    plugin->ConnectPort(portInfo.index(),(void*)(audioOutputs[ix].data()));
                }
            }
        }
    }
    void SetControl(const char*symbol, float value) {
        auto f = symbolToPortNumber.find(symbol);
        if (f == symbolToPortNumber.end()) {
            throw std::runtime_error("Port not found.");
        }
        controlData[f->second] = value;
    }
    float GetControl(const char*symbol) {
        auto f = symbolToPortNumber.find(symbol);
        if (f == symbolToPortNumber.end()) {
            throw std::runtime_error(std::string("Port not found.") + std::string(symbol));
        }
        return controlData[f->second];
    }
    float*GetInputAudio(int nPort) {
        return audioInputs[nPort].data();
    }
    const float*GetOutputAudio(int nPort) {
        return audioOutputs[nPort].data();
    }
    Lv2PluginInfo&info() { return pluginInfo; }
private:
    std::map<std::string,int> symbolToPortNumber;
    std::vector<float> controlData;
    std::vector<std::vector<float>> audioInputs;
    std::vector<std::vector<float>> audioOutputs;

    T pluginInfo;
    HostedLv2Plugin*plugin = nullptr;

};

static constexpr size_t MAX_BUFFER_SIZE = 512*1024;

static size_t msToSamples(float ms, double sampleRate) {
    return (size_t)(ms * 0.001 * sampleRate);
}
static void TestAttack(TestLv2Host<PluginInfo> &lv2Host)
{
    float *in = lv2Host.GetInputAudio(0);
    const float *out = lv2Host.GetOutputAudio(0);

    assert(lv2Host.GetControl("gate_level") < 0.0f);


    float attackMs = lv2Host.GetControl("attack");
    size_t attackSamples = msToSamples(attackMs, lv2Host.GetSampleRate());
    assert(attackSamples < MAX_BUFFER_SIZE);
    for (size_t i = 0; i < attackSamples; ++i)
    {
        in[i] = 1.0f;
    }


    lv2Host.Run((int)attackSamples);
    float lastValue = -1.0f;
    (void)lastValue;
    for (size_t i = 1; i < attackSamples; ++i)
    {
        assert(out[i] >= lastValue);
        lastValue = out[i];
    }
    assert(out[attackSamples-1] > 0.9);

    assert(std::abs(lv2Host.GetControl("gate_level")) < 1E-6);

    // finish the attack envelope.
    if (lv2Host.GetControl("gate_level") < 0) // one short due to rounding?
    {
        lv2Host.Run(1);
    }
    assert(std::abs(lv2Host.GetControl("gate_level")) == 0);

}


static void TestHold(TestLv2Host<PluginInfo> &lv2Host)
{
    float *in = lv2Host.GetInputAudio(0);
    const float *out = lv2Host.GetOutputAudio(0);
    (void)out;

    // finish the attack envelope.
    if (lv2Host.GetControl("gate_level") < 0) // one short due to rounding?
    {
        lv2Host.Run(1);
    }
    assert(std::abs(lv2Host.GetControl("gate_level")) == 0);


    float holdMs = lv2Host.GetControl("hold");
    size_t holdSamples = msToSamples(holdMs, lv2Host.GetSampleRate());
    assert(holdSamples < MAX_BUFFER_SIZE);


    float attackLevel = Db2Af(lv2Host.GetControl("threshold"),-96);
    float releaseLevel = Db2Af(lv2Host.GetControl("hysteresis"),-96)*attackLevel*0.5f;


    for (size_t i = 0; i < holdSamples+1; ++i)
    {
        in[i] = releaseLevel;
    }


    lv2Host.Run((int)holdSamples+1);
    for (size_t i = 0; i < holdSamples; ++i)
    {
        assert(out[i] == releaseLevel);
    }
    assert(out[holdSamples] < releaseLevel);

    assert(lv2Host.GetControl("gate_level") < 0);

}

static void TestRelease(TestLv2Host<PluginInfo> &lv2Host)
{
    TestHold(lv2Host);
    
    float *in = lv2Host.GetInputAudio(0);
    const float *out = lv2Host.GetOutputAudio(0);

    float releaseMs = lv2Host.GetControl("release");
    size_t releaseSamples = msToSamples(releaseMs, lv2Host.GetSampleRate());
    assert(releaseSamples < MAX_BUFFER_SIZE);


    float attackLevel = Db2Af(lv2Host.GetControl("threshold"),-96);
    float releaseLevel = Db2Af(lv2Host.GetControl("hysteresis"),-96)*attackLevel*0.5f;


    for (size_t i = 0; i < releaseSamples+1; ++i)
    {
        in[i] = releaseLevel;
    }


    lv2Host.Run((int)releaseSamples+1);
    float lastValue = out[0];
    (void)lastValue;
    for (size_t i = 1; i < releaseSamples; ++i)
    {
        assert(out[i] <= lastValue + 1E-9); // rounding errors due to code optimizations
        lastValue = out[i];

    }
    double expectedDb = lv2Host.GetControl("reduction");
    double actualDb = AF2Db(out[releaseSamples]/in[releaseSamples]);
    (void)expectedDb;
    (void)actualDb;
    assert(std::abs(expectedDb-actualDb) < 0.1);

    assert(std::abs(lv2Host.GetControl("gate_level")-expectedDb) < 0.1);

}

static void TestEnvelopes(TestLv2Host<PluginInfo> &lv2Host) {
    TestAttack(lv2Host);
    TestHold(lv2Host);
    TestAttack(lv2Host);
    TestRelease(lv2Host);
}
int main(int argc, char**argv) {
    TestLv2Host<PluginInfo> lv2Host("http://two-play.com/plugins/toob-noise-gate",48000,MAX_BUFFER_SIZE);



    lv2Host.SetControl("attack",3.0f);

    float *in = lv2Host.GetInputAudio(0);
    const float *out = lv2Host.GetOutputAudio(0);


    lv2Host.Activate();

    for (int i = 0; i < 512; ++i)
    {
        in[i] = 0;
    }
    lv2Host.Run(512);
    for (int i = 0; i < 512; ++i)
    {
        assert(out[i] == 0);
    }

    {
        lv2Host.SetControl("threshold",-12);
        lv2Host.SetControl("hysteresis",-30);
        lv2Host.SetControl("reduction",-12);
        lv2Host.SetControl("attack",500);
        lv2Host.SetControl("hold",1000);
        lv2Host.SetControl("release",5000);

        TestEnvelopes(lv2Host);
    }
   {
        lv2Host.SetControl("threshold",-30);
        lv2Host.SetControl("hysteresis",-12);
        lv2Host.SetControl("attack",3);
        lv2Host.SetControl("hold",30);
        lv2Host.SetControl("release",3000);
        lv2Host.SetControl("reduction",-60);

        TestEnvelopes(lv2Host);
    }
 
    (void)in;
    (void)out;

    
    


}