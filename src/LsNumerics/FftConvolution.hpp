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

#include <cstddef>
#include <complex>
#include <vector>
#include <limits>
#include <memory>
#include <cassert>
#include <cmath>
#include <map>
#include <mutex>

#pragma once
#ifndef FFT_CONVOLUTION__HPP
#define FFT_CONVOLUTION__HPP
namespace LsNumerics
{

    class FftConvolution
    {
        // tuning parameters.

    public:
        static constexpr std::size_t MINIMUM_DIRECT_CONVOLUTION_LENGTH = 0;
        static constexpr std::size_t MINIMUM_FFT_SIZE = 64;
        using complex_t = std::complex<double>;
        FftConvolution(std::vector<float> &samples);

        float Tick(float value)
        {
            delayLine.push(value);

            double result = 0;
            for (size_t i = 0; i < this->directConvolutionLength; ++i)
            {
                result += delayLine[i]*this->directImpulse[i];
            }

            for (size_t i = 0; i < sections.size(); ++i)
            {
                result += sections[i].Tick(delayLine);
            }
            return (float)result;
        }
        void Tick(const std::vector<float> &inputs, std::vector<float>&outputs)
        {
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                outputs[i] = Tick(inputs[i]);
            }
        }
    public:
        class DelayLine
        {
        public:
            DelayLine() { SetSize(0); }
            DelayLine(size_t size)
            {
                SetSize(size);
            }
            void SetSize(size_t size);

            void push(float value)
            {
                head = (head - 1) & size_mask;
                storage[head] = value;
            }
            float at(size_t index) const
            {
                return storage[(head + index) & size_mask];
            }

            float operator[](size_t index) const
            {
                return at(index);
            }

        private:
            std::vector<float> storage;
            std::size_t head = 0;
            std::size_t size_mask = 0;
        };

        // FFT specifically tweaked for Convolution.
        class FftPlan
        {
        public:
            using ptr = std::shared_ptr<FftPlan>;
            enum class FftDirection
            {
                Forward = +1,
                Backward = -1
            };

        private:
            static std::map<size_t, FftPlan::ptr> planCache;
            static std::mutex cacheMutex;

            std::vector<int> bitReverse;
            std::vector<complex_t> buffer;
            std::vector<complex_t> twiddleIncrements;

            double norm;
            int log2N;
            int N = -1;

        private:
        public:
            FftPlan() {}
            FftPlan(int size)
            {
                SetSize(size);
            }
            static ptr GetCachedPlan(size_t size);

            int GetSize() const { return N; }
            void SetSize(int size);

            void Compute(const std::vector<complex_t> &input, std::vector<complex_t> &output, FftDirection dir);


            void Compute(std::size_t offset,const std::vector<float> &input,std::vector<complex_t> &output, FftDirection direction)
            {
                for (int i = 0; i < N; ++i)
                {
                    if (i+offset >= input.size())
                    {
                        buffer[i] = 0;
                    } else {
                        buffer[i] = input[i+offset];
                    }
                }
                Compute(buffer, output, direction);
            }

            void Forward(const std::vector<float> &input, std::vector<complex_t> &output)
            {
                for (int i = 0; i < N; ++i)
                {
                    buffer[i] = input[i];
                }
                Compute(buffer, output, FftDirection::Forward);
            }
            void Compute(size_t offset, const DelayLine &delayLine, std::vector<complex_t> &output,FftDirection direction)
            {
                for (int i = 0; i < N; ++i)
                {
                    buffer[i] = delayLine[offset + i];
                }
                Compute(buffer, output, direction);
            }

            void Reverse(const std::vector<complex_t> &input, std::vector<complex_t> &output)
            {
                Compute(input, output, FftDirection::Backward);
            }
        };
    public:
        class Section
        {
        public:
            Section(
                size_t size, 
                size_t offset, 
                const std::vector<float> &impulseSamples
                );

            static size_t GetSectionDelay(size_t size) { return size; }

            size_t Size() const { return size; }
            size_t Delay() const { return size; }
            size_t InputOffset() const { return inputOffset; }

            float Tick(DelayLine &delayLine)
            {
                // XXX OPTIMIZE ME
                if (tickOffset == 0)
                {
                    Update(delayLine);
                }
                float result = (float)(buffer[tickOffset+size].real());
                ++tickOffset;
                if (tickOffset == size)
                {
                    tickOffset = 0;
                }
                return result;
            }
        private:
            void Update(const DelayLine&delayLine);
            size_t tickOffset = 0;
            size_t size;
            std::size_t sampleOffset;
            FftPlan::ptr fftPlan;
            std::vector<complex_t> impulseFft;
            std::vector<complex_t> buffer;
            std::size_t inputOffset;
            std::size_t directConvolutionLength;
        };
    private:
        DelayLine delayLine;
        std::vector<float> directImpulse;
        size_t directConvolutionLength;

        std::vector<Section> sections;
    };
}
#endif