// Copyright (c) 2025 Robin E. R. Davies
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

#include "ToobNoiseGate.hpp"
#include "lv2_plugin/Lv2Ports.hpp"

#include <limits>

ToobNoiseGate::ToobNoiseGate(double rate,
                             const char *bundle_path,
                             const LV2_Feature *const *features)
    : ToobNoiseGateBase(rate, bundle_path, features)
{
}

ToobNoiseGate::~ToobNoiseGate()
{
}

void ToobNoiseGate::Mix(uint32_t n_samples)
{
    const float *in = this->in.Get();
    float *out = this->out.Get();
 
    uint32_t ix = 0;

    while (ix < n_samples)
    {
        switch (gateState)
        {
        case GateState::Idle:
        {
            this->currentDb = this->reduction.GetDbNoLimit();
            float af = Db2AF(this->currentDb,-60); // clip to zero during idle
            while (ix < n_samples)
            {
                float v = in[ix];
                out[ix] = af * in[ix];
                ++ix;
                if (std::abs(v) >= this->thresholdValue)
                {
                    this->gateState = GateState::Attack;
                    this->currentDb = reduction.GetDbNoLimit();
                    if (attackSamples > 0) {
                        this->dxCurrentDb = -currentDb / attackSamples;
                        this->currentDb += this->dxCurrentDb;
                        this->samplesRemaining = attackSamples;
                    } else {
                        this->gateState = GateState::Hold;
                        this->dxCurrentDb = 0;
                        this->currentDb = 0;
                        this->samplesRemaining = holdSamples;
                    }
                    break;
                }
            }
        }
        break;
        case GateState::Attack:
        {
            while (ix < n_samples)
            {
                if (currentDb >= 0) {
                    break;
                }
                double v = in[ix] * Db2Af(currentDb,-192);
                out[ix] = (float)v;
                currentDb += this->dxCurrentDb;
                ++ix;
            }
            if (currentDb >= 0) {
                this->gateState = GateState::Hold;
                this->samplesRemaining = this->holdSamples;
                currentDb = 0;
                dxCurrentDb = 0;
                break;
            }
        }
        break;
        case GateState::Hold:
        {
            this->currentDb = 0;
            while (samplesRemaining && ix < n_samples)
            {
                float v = in[ix];
                out[ix] = v;
                ++ix;
                samplesRemaining--;                
                if (v >= hysteresisValue)
                {
                    this->samplesRemaining = this->holdSamples; // reset hold time.
                }
            }
            if (samplesRemaining == 0)
            {
                this->gateState = GateState::Release;
                this->samplesRemaining = this->releaseSamples;
                this->dxCurrentDb = this->reduction.GetDbNoLimit() / this->samplesRemaining;
                this->currentDb = this->dxCurrentDb;
                break;
            }
        }
        break;
        case GateState::Release:
        {
            float reductionDb = this->reduction.GetDbNoLimit();

            while (ix < n_samples)
            {
                if (this->currentDb <= reductionDb )
                {
                    break;
                }

                float v = in[ix];
                out[ix] = v * Db2AF(this->currentDb,-96);
                this->currentDb += this->dxCurrentDb;
                ++ix;
                if (std::abs(v) > thresholdValue)
                {
                    if (currentDb < -1E-7) {
                        this->gateState = GateState::Attack;
                        this->dxCurrentDb = -this->currentDb / this->attackSamples;
                        this->currentDb += this->dxCurrentDb;
                    } else {
                        this->gateState = GateState::Hold;
                        this->currentDb = 1.0f;
                        this->samplesRemaining = this->holdSamples;
                        
                    }
                    break;
                }
            }
            if (this->currentDb <= reductionDb )
            {
                this->gateState = GateState::Idle;
                this->currentDb = reductionDb;
                this->samplesRemaining = std::numeric_limits<int64_t>::max();
                break;
            }

        }
        break;
        }
    }
    this->gate_level.SetValue(this->currentDb,n_samples);

    if (this->gateState == GateState::Idle || this->gateState == GateState::Release)
    {
        this->trigger_led.SetValue(0.0f,n_samples);
    } else {
        this->trigger_led.SetValue(1.0f,n_samples);
    }
    
}

static size_t msToSamples(float msec, double rate)
{
    auto v = (size_t)(msec * 0.001 * rate);
    if (v == 0)
        return 1;
    return v;
}

void ToobNoiseGate::ResetGateState()
{
    this->gateState = GateState::Idle;
    this->currentDb = -96;
    this->dxCurrentDb = 0;
    this->samplesRemaining = std::numeric_limits<int64_t>::max();
}
void ToobNoiseGate::UpdateControls()
{
    this->thresholdValue = threshold.GetAfNoLimit();
    this->hysteresisValue = this->thresholdValue * hysteresis.GetAfNoLimit();
    this->reductionValue = this->reduction.GetAfNoLimit();

    this->attackSamples = msToSamples(this->attack.GetValue(), getRate());
    this->holdSamples = msToSamples(this->hold.GetValue(), getRate());
    this->releaseSamples = msToSamples(this->release.GetValue(), getRate());
}
void ToobNoiseGate::Run(uint32_t n_samples)
{
    UpdateControls();
    Mix(n_samples);
}

void ToobNoiseGate::Activate()
{
    super::Activate();
    UpdateControls();
}
void ToobNoiseGate::Deactivate()
{
    super::Deactivate();
}

REGISTRATION_DECLARATION PluginRegistration<ToobNoiseGate> toobNoiseGateRegistration(ToobNoiseGate::URI);
