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
#include "WavConstants.hpp"


namespace toob
{
    
    class ChebyshevDownsamplingFilter;

    // Position of an virtual ambisonic microphone.
    class AmbisonicMicrophone
    {
    public:
        
        /// @brief Constructor
        /// @param horizontalAngle Horizontal position of the virtual microphone in degrees.
        /// @param verticalAngle  Vertical position of the virtual microphone in degrees.
        /// @param micP Contols the polar pattern of the microphone. A value between zero and one. See remarks.
        /// @remarks
        /// The micP parameter controls the polar pattern of the microphone. A value of 0 produces a figure-of
        /// eight polar patern. 0.5 produces a cardiod pattern. 1.0 prodeuces an omnidirectional pattern. 
        /// See https://en.wikipedia.org/wiki/Ambisonics#:~:text=Virtual%20microphones for precise details.
        ///
        AmbisonicMicrophone(double horizontalAngle, double verticalAngle, double micP = 0.5)
            : horizontalAngle(horizontalAngle), verticalAngle(verticalAngle), micP(micP)
        {
        }
        double getHorizontalAngle() const
        {
            return horizontalAngle;
        }
        double getVerticalAngle() const
        {
            return verticalAngle;
        }
        double getMicP() const
        {
            return micP;
        }

    private:
        double horizontalAngle, verticalAngle, micP;
    };

    class AudioData
    {
    public:
        AudioData(size_t sampleRate = 0, size_t channelCount = 0, size_t size = 0)
        {
            this->sampleRate = sampleRate;
            setChannelCount(channelCount);
            setSize(size);
        }
        AudioData(size_t sampleRate, std::vector<float> &&samples)
        {
            this->sampleRate = sampleRate;
            setChannelCount(1);
            data[0] = std::move(samples);
            size = data.size() == 0 ? 0 : data.size();
        }
        AudioData(size_t sampleRate, std::vector<std::vector<float>> &&samples)
        {
            this->sampleRate = sampleRate;
            setChannelCount(1);
            data = std::move(samples);
            size = data.size() == 0 ? 0 : data.size();
        }
        AudioData(size_t sampleRate, const std::vector<float> &samples)
        {
            this->sampleRate = sampleRate;
            setChannelCount(1);
            data[0] = samples;
            size = data.size() == 0 ? 0 : data.size();
        }
        size_t getSize() const { return size; }
        void setSize(size_t size)
        {
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
        void setSampleRate(size_t sampleRate) { this->sampleRate = sampleRate; }
        size_t getChannelCount() const
        {
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
        void setChannelMask(ChannelMask channelMask) { this->channelMask = channelMask; }
        ChannelMask getChannelMask() const { return this->channelMask; }
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
            }
            else
            {
                size = this->data.size();
            }
        }
        void setData(std::vector<std::vector<float>> &&data)
        {
            this->data = std::move(data);
            if (this->data.size() == 0)
            {
                size = 0;
            }
            else
            {
                size = this->data[0].size();
            }
        }

        const std::vector<std::vector<float>> &getData() const { return data; }
        std::vector<std::vector<float>> &getData() { return data; }

        void ConvertToMono();

        void Scale(float value);

        AudioData&operator+=(const AudioData&other);


        
        /// @brief Downmix ambisonic data into a single channel
        /// @param micParameter The virtual microphone to use for the downmix.
        /// @return A downmixed channel of audio data.
        /// @remarks
        /// Current data must be 4-channel Ambisonic b-format audio data with channels in WXYZ order.
        std::vector<float> AmbisonicDownmixChannel(const AmbisonicMicrophone &micParameter);

        /// @brief Remix ambisonic audio data into channels for each of the suppled micParameters.
        /// @param micParameters The position of the microphone for each of the resulting channels.
        /// @remarks
        /// Current data must be 4-channel Ambisonic b-format audio data, with channels in WXYZ order.
        void AmbisonicDownmix(const std::vector<AmbisonicMicrophone> &micParameters);


        /// @brief Resample the audio data.
        /// @param outputSampleRate The new sample rate.
        /// @param output The AudioData object in which to store the result.

        void Resample(size_t outputSampleRate, AudioData &output);

        /// @brief Resample the audio data.
        /// @param outputSampleRate The new sample rate.
        void Resample(size_t outputSampleRate);


        /// @brief Remove samples from the audio data.
        /// @param start The start of samples to remove.
        /// @param end The end of samples to remove.
        void Erase(size_t start, size_t end);

    private:
        static std::vector<float> Resample(size_t inputSampleRate, size_t outputSampleRate, std::vector<float> &values);

        static ChebyshevDownsamplingFilter DesignFilter(size_t inputSampleRate, size_t outputSampleRate);
        static std::vector<float> Resample(size_t inputSampleRate, size_t outputSampleRate, std::vector<float> &values,ChebyshevDownsamplingFilter*downsamplingFilter);
        ChannelMask channelMask = ChannelMask::ZERO;
        size_t sampleRate = 22050;
        size_t size = 0;
        std::vector<std::vector<float>> data;
    };
}