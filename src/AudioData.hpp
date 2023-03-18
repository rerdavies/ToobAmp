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
#pragma once

#include <vector>
#include <cstddef>

namespace TwoPlay {
    class AudioData {
    public:
        AudioData(size_t  sampleRate = 0, size_t channelCount = 0, size_t size = 0) {
            this->sampleRate = sampleRate;
            setChannelCount(channelCount);
            setSize(size);

        }
        AudioData(size_t sampleRate, std::vector<float>&&samples)
        {
            this->sampleRate = sampleRate;
            setChannelCount(1);
            data[0] = std::move(samples);
            size = data.size() == 0? 0: data.size();
        }
        AudioData(size_t sampleRate, std::vector<std::vector<float>>&&samples)
        {
            this->sampleRate = sampleRate;
            setChannelCount(1);
            data = std::move(samples);
            size = data.size() == 0? 0: data.size();
        }
        AudioData(size_t sampleRate, const std::vector<float>&samples)
        {
            this->sampleRate = sampleRate;
            setChannelCount(1);
            data[0] = samples;
            size = data.size() == 0? 0: data.size();

        }
        size_t getSize() const { return size; }
        void setSize(size_t size) { 
            if (this->size != size)
            {
                this->size = size;
                for (size_t i = 0; i < getChannelCount(); ++i)
                {
                    data[i].resize(size);
                }
            }

        }

        size_t getSampleRate() const { return sampleRate; }
        void setSampleRate(size_t sampleRate) { this->sampleRate = sampleRate;}
        size_t getChannelCount() const {
            return data.size();
        }
        void setChannelCount(size_t channelCount)
        {
            size_t oldSize = data.size();
            if (channelCount != oldSize)
            {
                data.resize(channelCount);
                for (size_t i = oldSize; i < channelCount; ++i)
                {
                    data[i].resize(this->size);
                }
            }
        }
        const std::vector<float> &getChannel(size_t channel) const
        {
            return data[channel];
        }
        std::vector<float> &getChannel(size_t channel) 
        {
            return data[channel];
        }
        void setData(const std::vector<std::vector<float>> &data)
        {
            this->data = data;
            if (this->data.size() == 0)
            {
                size = 0;
            } else {
                size = this->data.size();
            }
        }
        void setData(std::vector<std::vector<float>> &&data)
        {
            this->data = std::move(data);
            if (this->data.size() == 0)
            {
                size = 0;
            } else {
                size = this->data.size();
            }
        }

        const std::vector<std::vector<float>>& getData() const { return data;}
        std::vector<std::vector<float>>& getData() { return data; }

        void ConvertToMono() {
            if (getChannelCount() > 1)
            {
                std::vector<float> mono(getSize());

                size_t channels = getChannelCount();
                float scale = 1.0f/channels;
                size_t size = getSize();
                for (size_t i = 0; i < size; ++i)
                {
                    float result = data[0][i];

                    for (size_t c = 1; c < channels; ++c)
                    {
                        result += data[c][i];
                    }
                    mono[i] = result*scale;
                }
                data[0] = std::move(mono);
                data.resize(1);
            }
        }

        void Resample(size_t outputSampleRate,AudioData &output);
        void Resample(size_t outputSampleRate);

        static std::vector<float> Resample(size_t inputSampleRate,size_t outputSampleRate,std::vector<float>&values);
    private:

        size_t sampleRate = 22050;
        size_t size = 0;
        std::vector<std::vector<float>> data;

    };
}