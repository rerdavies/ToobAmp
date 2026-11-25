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

#include "ToobCompressor.hpp"
#include "LsNumerics/Denorms.hpp"

ToobCompressor::ToobCompressor(double rate,
                 const char *bundle_path,
                 const LV2_Feature *const *features)
    : ToobCompressorBase(rate, bundle_path, features)
{
    compressor.Initialize(rate);
    trimDezipper.SetSampleRate(rate);
    trimDezipper.SetRate(0.1);
    outputDezipper.SetSampleRate(rate);
    outputDezipper.SetRate(0.1);
    compressorEnvelope.SetSampleRate(rate);

    const int32_t INPUT_UPDATES_PER_SEC = 15;
    vuMaxSampleCount = uint32_t(rate / INPUT_UPDATES_PER_SEC);
    trimOut.SetSampleRate(rate);
    dryWetDezipper.SetSampleRate(rate);

}

ToobCompressor::~ToobCompressor()
{
}

void ToobCompressor::Activate()
{
    compressor.Reset();
    trimDezipper.Reset(this->trim.GetDb());
    outputDezipper.Reset(this->output.GetDb());
    trimOut.Reset();
    compressor.SetAttackTime(this->attack.GetValue()*0.001);
    compressor.SetReleaseTime(this->release.GetValue()*0.001);
}
void ToobCompressor::Deactivate()
{
}

void ToobCompressor::UpdateControls() {
    if (trim.HasChanged())
    {
        trimDezipper.SetTarget(trim.GetDb());
    }
    if (output.HasChanged())
    {
        outputDezipper.SetTarget(output.GetDb());
    }
    if (attack.HasChanged())
    {
        compressor.SetAttackTime(attack.GetValue()*0.001);
    }
    if (release.HasChanged())
    {
        compressor.SetReleaseTime(attack.GetValue()*0.001);
    }
    if (dryWet.HasChanged())
    {
        dryWetDezipper.To(dryWet.GetValue(),0.1);
    }
}


static float gInputVuAdjustment = Db2AF(6.0f,-96.0f);
void ToobCompressor::CaptureVu(float value) {
    value = std::abs(value)*gInputVuAdjustment;
    trimOut.AddValue(value);
}

void ToobCompressor::CaptureVu(float valueL, float valueR)
{
    double value = std::max(std::abs(valueL), std::abs(valueR))*gInputVuAdjustment;
    trimOut.AddValue(value);
}

void ToobCompressor::Compressor(uint32_t n_samples)
{
    const float*inL = this->inL.Get();
    const float *inR = this->inR.Get();
    if (inR) {
        // stereo.
        float*outL = this->outL.Get();
        float *outR = this->outR.Get();
        for (size_t i = 0; i < n_samples; ++i)
        {
            auto trim = this->trimDezipper.Tick();
            auto output = this->outputDezipper.Tick();
            auto dryWetValue = this->dryWetDezipper.Tick();

            float inLValue = inL[i]*trim;
            float inRValue = inR[i]*trim;
            CaptureVu(inLValue,inRValue);
            auto result = compressor.Tick(inLValue,inRValue);
            outL[i] = (dryWetValue*result.left*output + (1.0f-dryWetValue)*inLValue)
               ;
            outR[i] = (dryWetValue*result.right*output + (1.0f-dryWetValue)*inRValue)
               ;
        }
        this->compressorEnvelope.SetValue(compressor.EnvelopeVoltage(),n_samples);
    } else {
        float*outL = this->outL.Get();
        for (size_t i = 0; i < n_samples; ++i)
        {
            auto trim = this->trimDezipper.Tick();
            auto output = this->outputDezipper.Tick();
            float dryWetValue = dryWetDezipper.Tick();

            float inLValue = inL[i]*trim;
            CaptureVu(inLValue);
            float outValue = compressor.Tick(inLValue);
            outL[i] = (dryWetValue*(outValue*output) + (1.0f-dryWetValue)*inLValue)
               ;
        }
    }
}

void ToobCompressor::Run(uint32_t n_samples)
{
    fp_state_t fp_state = LsNumerics::disable_denorms();

    UpdateControls();
    Compressor(n_samples);

    LsNumerics::restore_denorms(fp_state);

}



REGISTRATION_DECLARATION PluginRegistration<ToobCompressor> toobCompressorRegistration(ToobCompressor::URI);
