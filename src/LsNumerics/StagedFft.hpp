/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:

 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.

 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#pragma once

#ifndef LSNUMERICS_INCLUDE_STAGEDFFT_HPP
#define LSNUMERICS_INCLUDE_STAGEDFFT_HPP

#include <complex>
#include <vector>
#include <memory>
#include <mutex>

#include <cmath>
#include <cstdint>
#include <cassert>
#include "LsMath.hpp"
#include "Window.hpp"
#include <functional>

#ifndef RESTRICT
#define RESTRICT __restrict // equivalent of C99 restrict keyword. Valid for MSVC,GCC and CLANG.
#endif
namespace LsNumerics
{
    namespace Implementation
    {
        class StagedFftPlan
        {
        public:
            // FFT direction specifier
            using complex_t = std::complex<double>;

            enum class Direction {
                Forward = +1,
                Backward = -1
            };

            class InstanceData
            {
            public:
                InstanceData(size_t size)
                {
                    workingBuffer.resize(size);
                }
                void SetSize(size_t size)
                {
                    workingBuffer.resize(size);
                }
                std::vector<complex_t> &GetWorkingBuffer() { return workingBuffer; }

            private:
                std::vector<complex_t> workingBuffer;
            };

        private:
            StagedFftPlan(size_t size)
            {
                SetSize(size);
            }

        public:
            StagedFftPlan() = delete;
            StagedFftPlan(const StagedFftPlan &) = delete;

            static StagedFftPlan &GetCachedInstance(size_t size);

            size_t GetSize() const { return fftSize; }
            void SetSize(size_t size);

            void Compute(InstanceData &instanceData, const std::vector<complex_t> &input, std::vector<complex_t> &output, Direction dir);

            void Compute(InstanceData &instanceData, const std::vector<float> &input, std::vector<complex_t> &output, Direction dir);

            bool IsL1Optimized() const {
                return isL1Optimized;
            }
            bool IsL2Optimized() const {
                return isL2Optimized;
            }

        private:
            bool isL1Optimized = false;
            bool isL2Optimized = false;
            static std::recursive_mutex cacheMutex;
            static std::vector<std::unique_ptr<StagedFftPlan>> cache;

            // minimal implementation of the subrange of a vector.
            template <typename T>
            class VectorRange
            {
            public:
                VectorRange(size_t start, size_t end, std::vector<T> &vector)
                {
                    assert(start < vector.size());
                    assert(end <= vector.size());
                    assert(start <= end);
                    p = (&vector[0]) + start;
                    _size = end - start;
                }
                VectorRange(size_t start, size_t end, const VectorRange<T> &vector)
                {
                    assert(start < vector.size());
                    assert(end <= vector.size());
                    assert(start <= end);
                    p = const_cast<T *>(vector.p) + start;
                    _size = end - start;
                }

                // conversion constructor.
                VectorRange(std::vector<T> &vector)
                    : VectorRange(0, vector.size(), vector)
                {
                }

                size_t size() const { return _size; }
                T &at(size_t index) const
                {
                    assert(index < _size);
                    return const_cast<T *>(p)[index];
                }
                T &operator[](size_t index) const
                {
                    return at(index);
                }

                using iterator_t = T *;

                iterator_t begin() { return p; }
                iterator_t end() { return p + size; }

            private:
                size_t _size;
                const T *p;
            };

            void ComputePass(size_t pass, const VectorRange<complex_t> &output, Direction dir);
            void ComputeInnerLarge(size_t pass, const VectorRange<complex_t> &output, Direction dir);
            void ComputeInner0(const VectorRange<complex_t> &output, Direction dir);
            void ComputeInner(InstanceData &instanceData, const VectorRange<complex_t> &output, Direction dir);
            void TransposeOutputs(InstanceData &instanceData, size_t cacheSize, size_t size, const VectorRange<complex_t> &outputs, Direction dir);

        private:
            using FftOp = std::function<void(InstanceData &instanceData, const VectorRange<complex_t> &output, Direction dir)>;

            StagedFftPlan *cacheEfficientFft = nullptr;

            std::vector<FftOp> ops;

            static constexpr size_t UNINITIALIZED_VALUE = (size_t)-1;
            std::vector<complex_t> forwardTwiddle;
            std::vector<complex_t> backwardTwiddle;
            std::vector<uint32_t> bitReverse;
            std::vector<std::pair<uint32_t, uint32_t>> reverseBitPairs;
            std::vector<uint32_t> reverseBitSelfPairs;
            double norm;
            size_t log2N;
            size_t fftSize = UNINITIALIZED_VALUE;

            void CalculateTwiddleFactors(Direction dir, std::vector<complex_t> &twiddles);
        };
    }

    class StagedFft
    {
    public:
        using complex_t = std::complex<double>;
        using Direction = Implementation::StagedFftPlan::Direction;

        StagedFft(size_t size)
            : plan(&Implementation::StagedFftPlan::GetCachedInstance(size)),
              instanceData(size)

        {
        }
        StagedFft() 
        :plan(nullptr),
         instanceData(0)
        {

        }
        void SetSize(size_t size)
        {
            plan = &Implementation::StagedFftPlan::GetCachedInstance(size);
            instanceData.SetSize(size);
        }        
        size_t GetSize() const {
            if (!plan) return 0;
            return plan->GetSize();
        }
        void Compute(const std::vector<float> &input, std::vector<complex_t> &output, Direction direction)
        {
            if (plan) // zero-length Compute does nothing.
            {
                plan->Compute(instanceData, input, output, direction);
            }
        }
        void Compute(const std::vector<complex_t> &input, std::vector<complex_t> &output, Direction direction)
        {
            if (plan)
            {
                plan->Compute(instanceData, input, output, direction);
            }
        }

        void Forward(const std::vector<complex_t> &input, std::vector<complex_t> &output)
        {
            Compute(input,output,Direction::Forward);
        }
        void Backward(const std::vector<complex_t> &input, std::vector<complex_t> &output)
        {
            Compute(input,output,Direction::Backward);
        }

        bool IsL1Optimized() const { return plan->IsL1Optimized(); }
        bool IsL2Optimized() const { return plan->IsL2Optimized(); }

    private:
        using InstanceData = Implementation::StagedFftPlan::InstanceData;
        Implementation::StagedFftPlan *plan;
        InstanceData instanceData;
    };

} // namespace

#endif // DJ_INCLUDE_FFT_H
