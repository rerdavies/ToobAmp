/*
 * MIT License
 * 
 * Copyright (c) 2023 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <numbers>
#include "AudioData.hpp"
#include "LsNumerics/LagrangeInterpolator.hpp"
#include "Filters/ChebyshevDownsamplingFilter.h"
#include "LsNumerics/LsMath.hpp"

using namespace TwoPlay;
using namespace LsNumerics;


void AudioData::Resample(size_t outputSampleRate,AudioData &output)
{

    output.setSampleRate(getSampleRate());
    output.setChannelCount(getChannelCount());

    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(),outputSampleRate,getChannel(c));
        output.getData()[c] = std::move(resampledChannel);
    }
    output.size = output.data[0].size();
}
void AudioData::Resample(size_t sampleRate)
{
    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(),sampleRate,getChannel(c));
        data[c] = std::move(resampledChannel);
    }
    this->sampleRate = sampleRate;
    if (data.size() == 0)
    {
        this->size = 0;
    } else {
        this->size = data[0].size();
    }
}

std::vector<float> AudioData::Resample(size_t inputSampleRate,size_t outputSampleRate,std::vector<float>&values)
{
    if (inputSampleRate == outputSampleRate)
    {
        return values;
    }
    size_t ORDER = (size_t)std::round(LsNumerics::Pi*10);
    LagrangeInterpolator interpolator{ORDER};
    if (outputSampleRate < inputSampleRate)
    {
        ChebyshevDownsamplingFilter downsamplingFilter;
        size_t cutoff;
        if (outputSampleRate < 48000)
        {
            cutoff = outputSampleRate*18000/44100; // 18000 for 44100, proportionately scaled if the sample rate is even lower.
        } else {
            cutoff = 20000;
        }
        downsamplingFilter.Design(inputSampleRate,0.5,cutoff,-6,outputSampleRate/2);

        std::vector<float> filteredData(values.size()+ORDER/2);

        // reduce upward ringing on the first sample.
        for (size_t i = 0; i < 500; ++i)
        {
            downsamplingFilter.Tick(values[0]); 
        }
        for (size_t i = 0; i < values.size(); ++i)
        {
            filteredData[i] = downsamplingFilter.Tick(values[i]);
        }
        for (size_t i = values.size(); i < filteredData.size(); ++i)
        {
            filteredData[i] = downsamplingFilter.Tick(0);
        }



        size_t newLength = std::ceil(values.size()*(double)outputSampleRate/inputSampleRate)+ORDER/2;
        std::vector<float> result(newLength);
        double dx = inputSampleRate/(double)outputSampleRate;

        size_t outputIndex = 0;
        double x = 0;
        for (size_t i = 0; i < newLength; ++i)
        {
            result[outputIndex++] = interpolator.Interpolate(values,x);
            x += dx;
        }
        return result;
    } else {
        size_t newLength = std::ceil(values.size()*(double)outputSampleRate/inputSampleRate)+ORDER/2;
        std::vector<float> result(newLength);
        double dx = inputSampleRate/(double)outputSampleRate;

        size_t outputIndex = 0;
        double x = 0;
        for (size_t i = 0; i < newLength; ++i)
        {
            result[outputIndex++] = interpolator.Interpolate(values,x);
            x += dx;
        }
        return result;
    }


}

