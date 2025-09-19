// Copyright (c) Robin E. R. Davies
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
#include "ToobTremoloInfo.hpp"
#include "ControlDezipper.h"
#include "Filters/LowPassFilter.h"
#include "Filters/HighPassFilter.h"
#include <cmath>
#include <cassert>

using namespace lv2c::lv2_plugin;
using namespace tremolo_plugin;
using namespace toob;

namespace tremolo_plugin
{

    // Chamberlin two-pole state filter. 
    // Hal Chamberlin, “Musical Applications of Microprocessors,” 2nd Ed, Hayden
    class StateFilter {
    public:
        void SetSampleRate(double rate) { this->fs = rate; }
        void SetCutoff(double fC) {
            this->f = 2 *sin (M_PI * fC / fs);
            this->q = 1.0;
            this->scale = std::sqrt(q);
        }

        void Reset() {
            low = 0; high = 0; band = 0;
            lowR = 0; highR = 0; bandR = 0;
        }
        void Tick(float input, float*lowOut, float *highOut) {
            low = low + f*band;
            high = scale * input - low-q*band;
            band = f*high+band;
            *lowOut = low;
            *highOut = high;
        }
        void TickR(float input, float*lowOut, float *highOut) {
            lowR = lowR + f*bandR;
            highR = scale * input - lowR-q*bandR;
            bandR = f*highR+bandR;
            *lowOut = low;
            *highOut = high;
        }

    private:
        double low = 0;
        double band = 0;
        double high = 0;

        double lowR = 0;
        double bandR = 0;
        double highR = 0;


        double q = 0;

        double f = 0;
        double fs = 0;
        double scale = 0;
    };
    class ShapeMap
    {
    public:
        ShapeMap() { SetShape(0); }
        float Map(float value)
        {
            double result = cY + mY * std::atan(cX + value * mX) + 1E-9; // 1E-9 to avoid negative values due to roundoff.
            return result;
        }
        void SetShape(float value)
        {
            double scale = Db2Af(-50 + value * 70) * 2 * M_PI;
            // solve:
            // cx + mX*1 = scale;
            // cX + mX*0 = -scale/2;
            // cx = -scale/2
            // -scale/2 + mX = scale
            // mY = 3*scale/2;
            cX = -scale / 2;
            mX = 3 * scale / 2;

            // check: cx+mY = scale
            assert(std::abs((cX + mX) - scale) < 1E-7);

            double y0 = std::atan(cX);
            double y1 = std::atan(mX + cX);
            // solve:
            // cY + mY *y1 = 1
            // cy + mY*y0 = 0
            // mY*(y1-y0) = 1 - (0)
            mY = 1 / (y1 - y0);
            cY = 1 - mY * y1;

            // check: mY*y1 +cY = 1
            assert(std::fabs((mY * y1 + cY) - 1) < 1E-7);
            // check: mY*y0 + cY = -1
            assert(std::fabs((mY * y0 + cY) - (0)) < 1E-7);
            // check: Shape(0) == 0
            assert(std::fabs(Map(0) - 0) < 1E-7);
            assert(Map(0) >= 0);
            assert(std::fabs(Map(1) - 1) < 1E-7);
        }

    private:
        double mX = 0, cX = 0, mY = 0, cY = 0;
    };

    class SinLfo
    {
    public:
        void SetFrequency(double f)
        {
            rate = (float)f;
        }
        void SetRate(float hz)
        {
            dT = 2 * M_PI * hz / rate;
        }
        float Tick()
        {
            double result = std::sin(t);
            t += dT;
            if (t > 2 * M_PI)
            {
                t -= 2 * M_PI;
            }
            return result;
        }

        float rate;
        float t = 0;
        float dT;
    };

};

class ToobTremolo : public ToobTremoloBase
{
public:
    using super = ToobTremoloBase;

    static Lv2Plugin *Create(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    {
        return new ToobTremolo(rate, bundle_path, features);
    }
    ToobTremolo(double rate,
                const char *bundle_path,
                const LV2_Feature *const *features);

    virtual ~ToobTremolo();

    static constexpr const char *URI = "http://two-play.com/plugins/toob-tremolo";

protected:
    virtual void Run(uint32_t n_samples) override;

    virtual void Activate() override;
    virtual void Deactivate() override;

    void OnPatchGet(LV2_URID propertyUrid) override;

private:
    ControlDezipper depthDezipper;
    LV2_URID tremolo__lfoShape = 0;
    int64_t requestLfoShapeCount = 0;
    bool sendLfoShape = false;

    void WriteLfoShape();

    void RequestDelayedLfoShape();
    virtual void RunNormalStereo(uint32_t n_samples);
    virtual void RunHarmonicStereo(uint32_t n_samples);
    virtual void RunNormalMono(uint32_t n_samples);
    virtual void RunHarmonicMono(uint32_t n_samples);

    ShapeMap shapeMap;


    LowPassFilter lowPass;
    HighPassFilter midHighPass;
    LowPassFilter midLowPass;
    HighPassFilter highPass;
    tremolo_plugin::SinLfo sinLfo;
};

class ToobTremoloMono : public ToobTremolo
{
public:
    using super = ToobTremolo;

    static constexpr const char *URI = "http://two-play.com/plugins/toob-tremolo-mono";

    static Lv2Plugin *Create(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    {
        return new ToobTremolo(rate, bundle_path, features);
    }
    ToobTremoloMono(double rate,
                    const char *bundle_path,
                    const LV2_Feature *const *features);

    virtual ~ToobTremoloMono();

    enum class PortId
    {
        rate = 0,
        depth = 1,
        harmonic = 2,
        shape = 3,
        inl = 4,
        outl = 5,
        control = 6,
        notify = 7,
    };

    virtual void ConnectPort(uint32_t port, void *data) override
    {
        switch ((PortId)port)
        {
        case PortId::rate:
            rate.SetData(data);
            break;
        case PortId::depth:
            depth.SetData(data);
            break;
        case PortId::harmonic:
            harmonic.SetData(data);
            break;
        case PortId::shape:
            shape.SetData(data);
            break;
        case PortId::inl:
            inl.SetData(data);
            break;
        case PortId::outl:
            outl.SetData(data);
            break;
        case PortId::control:
            control.SetData(data);
            break;
        case PortId::notify:
            notify.SetData(data);
            break;
        default:
            LogError("Invalid port id");
            break;
        }
    }
};