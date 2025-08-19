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
#include <iostream>
#include "ss.hpp"


#if USE_SECRET_RABBIT_RESAMPLER
#include "samplerate.h"
#endif
using namespace toob;
using namespace LsNumerics;

void AudioData::Resample(double outputSampleRate, AudioData &output)
{

    output.setSampleRate(outputSampleRate);
    output.setChannelCount(getChannelCount());
#if USE_SECRET_RABBIT_RESAMPLER
    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(), outputSampleRate, getChannel(c));
        output.getData()[c] = std::move(resampledChannel);
    }
    

#else

    if (outputSampleRate < getSampleRate())
    {
        auto downsamplingFilter = DesignFilter(getSampleRate(), outputSampleRate);
        for (size_t c = 0; c < getChannelCount(); ++c)
        {
            std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(), outputSampleRate, getChannel(c), &downsamplingFilter);
            output.getData()[c] = std::move(resampledChannel);
        }
    }
    else
    {
        for (size_t c = 0; c < getChannelCount(); ++c)
        {
            std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(), outputSampleRate, getChannel(c), nullptr);
            output.getData()[c] = std::move(resampledChannel);
        }
    }
#endif
    output.size = output.data[0].size();
}
void AudioData::Resample(double sampleRate)
{

#if USE_SECRET_RABBIT_RESAMPLER
    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(), sampleRate, getChannel(c));
        data[c] = std::move(resampledChannel);
    }
#else
    if (sampleRate < getSampleRate())
    {
        auto downsamplingFilter = DesignFilter(getSampleRate(), sampleRate);

        for (size_t c = 0; c < getChannelCount(); ++c)
        {
            std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(), sampleRate, getChannel(c), &downsamplingFilter);
            data[c] = std::move(resampledChannel);
        }
    }
    else
    {
        for (size_t c = 0; c < getChannelCount(); ++c)
        {
            std::vector<float> resampledChannel = AudioData::Resample(getSampleRate(), sampleRate, getChannel(c), nullptr);
            data[c] = std::move(resampledChannel);
        }
    }
#endif
    this->sampleRate = sampleRate;
    if (data.size() == 0)
    {
        this->size = 0;
    }
    else
    {
        this->size = data[0].size();
    }
}

struct ChannelMatrixValue
{
    ChannelMask channel;
    float scale;
};

static constexpr float SQRT2 = 1.4142135623730950488016887242097f;
// static constexpr float INV_SQRT2  = (float)(1.0 / SQRT2);
// static constexpr float INV_2SQRT2  = (float)(1.0 / (2* SQRT2));

std::vector<ChannelMatrixValue> monoMatrix = {
    {ChannelMask::SPEAKER_FRONT_LEFT, 1.0f / SQRT2},
    {ChannelMask::SPEAKER_FRONT_RIGHT, 1.0f / SQRT2},
    {ChannelMask::SPEAKER_FRONT_CENTER, 1.0f},
    {ChannelMask::SPEAKER_SIDE_LEFT, 1.0f / 2.0f},
    {ChannelMask::SPEAKER_SIDE_RIGHT, 1.0f / 2.0f},
    {ChannelMask::SPEAKER_BACK_LEFT, 1.0f / 2.0f},
    {ChannelMask::SPEAKER_BACK_RIGHT, 1.0f / 2.0f},
    {ChannelMask::SPEAKER_TOP_FRONT_LEFT, 1.0f / 2.0f},
    {ChannelMask::SPEAKER_TOP_FRONT_CENTER, 1.0f / SQRT2},
    {ChannelMask::SPEAKER_TOP_FRONT_RIGHT, 1.0f / 2.0f},
    {ChannelMask::SPEAKER_TOP_BACK_LEFT, 1.0f / (2 * SQRT2)},
    {ChannelMask::SPEAKER_TOP_BACK_RIGHT, 1.0f / (2 * SQRT2)},
};

static float GetMonoChannelDownmix(size_t channel, ChannelMask mask)
{
    ChannelMask thisChannel = GetChannel(channel, mask);
    for (auto &entry : monoMatrix)
    {
        if (entry.channel == thisChannel)
        {
            return entry.scale;
        }
    }
    return 0;
}
void AudioData::ConvertToMono()
{
    if (getChannelCount() > 1)
    {
        // https://www.audiokinetic.com/en/library/edge/?source=Help&id=downmix_tables
        if (channelMask != ChannelMask::ZERO)
        {
            try
            {
                std::vector<float> scale;
                scale.resize(getChannelCount());
                for (size_t i = 0; i < getChannelCount(); ++i)
                {
                    scale[i] = GetMonoChannelDownmix(i, channelMask);
                }
                for (size_t i = 0; i < size; ++i)
                {
                    float sum = 0;
                    for (size_t c = 0; c < getChannelCount(); ++c)
                    {
                        sum += this->data[c][i] * scale[i];
                    }
                    this->data[0][i] = sum;
                }
                data.resize(1);
                return;
            }
            catch (...)
            {
                // presumably, the channel mask doens't match the number of channels. fall through.
            }
        }
        if (getChannelCount() == 2)
        {
            std::vector<float> mono(getSize());

            size_t channels = getChannelCount();
            float scale = 1.0f / channels;
            size_t size = getSize();
            for (size_t i = 0; i < size; ++i)
            {
                float result = data[0][i];

                for (size_t c = 1; c < channels; ++c)
                {
                    result += data[c][i];
                }
                mono[i] = result * scale;
            }
            data[0] = std::move(mono);
            data.resize(1);
        }
        else
        {
            // just take the first channel.
            data.resize(1);
        }
    }
}

#if !USE_SECRET_RABBIT_RESAMPLER
/*static*/ ChebyshevDownsamplingFilter AudioData::DesignFilter(size_t inputSampleRate, size_t outputSampleRate)
{
    ChebyshevDownsamplingFilter downsamplingFilter;
    double cutoff;
    if (outputSampleRate < 48000)
    {
        cutoff = outputSampleRate * 20000.0 / 44100; // 18000 for 44100, proportionately scaled if the sample rate is even lower.
    }
    else
    {
        cutoff = outputSampleRate * 20000.0 / 44100;
        ;
    }
    downsamplingFilter.Design(inputSampleRate, 0.1, cutoff, -20, outputSampleRate / 2);
    return downsamplingFilter;
}
#endif

std::vector<float> AudioData::Resample(double inputSampleRate, double outputSampleRate, std::vector<float> &values)
{
#if USE_SECRET_RABBIT_RESAMPLER
    return Resample2(inputSampleRate, outputSampleRate, values);
#else 
    if (outputSampleRate < inputSampleRate)
    {
        ChebyshevDownsamplingFilter downsamplingFilter = DesignFilter(inputSampleRate, outputSampleRate);
        return Resample(inputSampleRate, outputSampleRate, values, &downsamplingFilter);
    }
    else
    {
        return Resample(inputSampleRate, outputSampleRate, values, nullptr);
    }
#endif
}

#if USE_SECRET_RABBIT_RESAMPLER
std::vector<float> AudioData::Resample2(double inputSampleRate, double outputSampleRate, std::vector<float> &values)
{
    if (inputSampleRate == outputSampleRate)
    {
        return values;
    }
    std::vector<float> result;

    size_t outputLength = (size_t)(values.size() * outputSampleRate / inputSampleRate);
    result.resize(outputLength);

    SRC_DATA data;
    data.data_in = values.data();
    data.input_frames = values.size();
    data.data_out = result.data();
    data.output_frames = result.size();
    data.input_frames_used = 0;
    data.output_frames_gen = 0;
    data.end_of_input = 0;
    data.src_ratio = outputSampleRate * 1.0 / inputSampleRate;

    int rc = src_simple(&data, SRC_SINC_MEDIUM_QUALITY, 1);
    if (rc != 0)
    {
        throw std::runtime_error(SS("Sample rate conversion failed." << src_strerror(rc)));
    }
    if ((size_t)data.output_frames_gen != result.size())
    {
        result.resize((size_t)data.output_frames_gen);
    }
    return result;
}

#else
std::vector<float> AudioData::Resample(size_t inputSampleRate, size_t outputSampleRate, std::vector<float> &values, ChebyshevDownsamplingFilter *downsamplingFilter)
{
    if (inputSampleRate == outputSampleRate)
    {
        return values;
    }
    size_t ORDER = (size_t)std::round(LsNumerics::Pi * 10);
    LagrangeInterpolator interpolator{ORDER};

    if (downsamplingFilter != nullptr)
    {
        downsamplingFilter->Reset();
        std::vector<float> filteredData(values.size() + ORDER / 2);

        // reduce upward ringing on the first sample.
        for (size_t i = 0; i < 500; ++i)
        {
            downsamplingFilter->Tick(values[0]);
        }
        for (size_t i = 0; i < values.size(); ++i)
        {
            filteredData[i] = downsamplingFilter->Tick(values[i]);
        }
        for (size_t i = values.size(); i < filteredData.size(); ++i)
        {
            filteredData[i] = downsamplingFilter->Tick(0);
        }

        size_t newLength = (size_t)std::ceil(values.size() * (double)outputSampleRate * 1.0f / inputSampleRate) + ORDER / 2;
        std::vector<float> result(newLength);
        double dx = inputSampleRate / (double)outputSampleRate;

        size_t outputIndex = 0;
        double x = 0;
        for (size_t i = 0; i < newLength; ++i)
        {
            result[outputIndex++] = interpolator.Interpolate(filteredData, x);
            x += dx;
        }
        return result;
    }
    else
    {
        size_t newLength = std::ceil(values.size() * (double)outputSampleRate / inputSampleRate) + ORDER / 2;
        std::vector<float> result(newLength);
        double dx = inputSampleRate / (double)outputSampleRate;

        size_t outputIndex = 0;
        double x = 0;
        for (size_t i = 0; i < newLength; ++i)
        {
            result[outputIndex++] = interpolator.Interpolate(values, x);
            x += dx;
        }
        return result;
    }
}
#endif

static double degreesToRadians(double degrees)
{
    return degrees * LsNumerics::Pi / 180.0;
}
std::vector<float> AudioData::AmbisonicDownmixChannel(const AmbisonicMicrophone &micParameter)
{
    assert(getChannelCount() == 4);
    std::vector<float> result;
    result.resize(size);

    double p = micParameter.getMicP();
    double w = p * std::sqrt(2.0);
    double x = -(1 - p) * std::cos(degreesToRadians(micParameter.getHorizontalAngle()));
    double y = -(1 - p) * std::sin(degreesToRadians(micParameter.getHorizontalAngle()));

    // std::cout << "a: " << micParameter.getHorizontalAngle() << " w: " << w << " x: " << x << " y: " << y << std::endl;

    const std::vector<float> &W = getChannel(0);
    const std::vector<float> &X = getChannel(1);
    const std::vector<float> &Y = getChannel(2);
    for (size_t i = 0; i < size; ++i)
    {
        result[i] = w * W[i] + x * X[i] + y * Y[i];
    }

    return result;
}

void AudioData::AmbisonicDownmix(const std::vector<AmbisonicMicrophone> &micParameters)
{
    assert(getChannelCount() == 4);
    std::vector<std::vector<float>> outputData;
    outputData.reserve(micParameters.size());
    for (size_t i = 0; i < micParameters.size(); ++i)
    {
        outputData.push_back(AmbisonicDownmixChannel(micParameters[i]));
    }
    this->data = std::move(outputData);
}

void AudioData::Erase(size_t start, size_t end)
{
    if (end <= start)
        return;
    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        std::vector<float> &channel = getChannel(c);
        channel.erase(channel.begin() + start, channel.begin() + end);
    }
}

void AudioData::Scale(float value)
{
    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        auto &channel = this->data[c];
        for (size_t i = 0; i < channel.size(); ++i)
        {
            channel[i] *= value;
        }
    }
}

void AudioData::MonoToStereo()
{
    this->data.resize(2);
    this->data[1] = this->data[0];
    this->channelMask = ChannelMask::SPEAKER_FRONT_LEFT | ChannelMask::SPEAKER_FRONT_RIGHT;
}

void AudioData::SetStereoWidth(float width)
{
    this->data.resize(2);
    float fLL = width * 0.5f + 0.5f;
    float fLR = -width * 0.5f + 0.5f;
    float fRL = -width * 0.5 + 0.5f;
    float fRR = width * 0.5f + 0.5f;

    auto &left = data[0];
    auto &right = data[1];
    size_t size = data[0].size();

    for (size_t i = 0; i < size; ++i)
    {
        float lVal = left[i] * fLL + right[i] * fLR;
        float rVal = left[i] * fRL + right[i] * fRR;
        left[i] = lVal;
        right[i] = rVal;
    }
}

AudioData &AudioData::operator+=(const AudioData &other)
{
    assert(this->getChannelCount() == other.getChannelCount());

    if (other.getSize() > this->getSize())
    {
        this->setSize(other.getSize());
    }
    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        auto &myChannel = data[c];
        const auto &theirChannel = other.getChannel(c);

        for (size_t i = 0; i < theirChannel.size(); ++i)
        {
            myChannel[i] += theirChannel[i];
        }
    }

    return *this;
}

void AudioData::InsertZeroes(size_t start, size_t count)
{
    if (count == 0)
        return;

    this->setSize(getSize() + count);
    for (size_t c = 0; c < getChannelCount(); ++c)
    {
        auto &chan = this->data[c];

        float *source = chan.data() + start;
        float *dest = chan.data() + start + count;
        size_t n = chan.size() - start - count;
        memmove(dest, source, n * sizeof(float)); // (maximally efficient)

        for (size_t i = 0; i < count; ++i)
        {
            chan[start + i] = 0;
        }
    }
}
