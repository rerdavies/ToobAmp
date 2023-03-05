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

/* DO NOT USE!!!

  PERFORMANCE IS ABOUT 6x worse than direct FFT convolution, and 
  does not yeilds 150% of real-time CPU use for very reasonable
  cases.
*/

#pragma once
#include <cstddef>
#include <complex>
#include <vector>
#include <limits>
#include <memory>
#include <cassert>
#include <cmath>
#include <unordered_map>

#ifndef RESTRICT
#define RESTRICT __restrict // good for MSVC, and GCC.
#endif

namespace LsNumerics
{
    using fft_float_t = double;
    using fft_complex_t = std::complex<fft_float_t>;
    using fft_index_t = int32_t;
    static constexpr fft_index_t CONSTANT_INDEX = -1;
    static constexpr fft_index_t INVALID_INDEX = -2;

    enum class FftDirection
    {
        Forward = 1,
        Reverse = -1
    };

    namespace Implementation
    {
        void SlotUsageTest();
        
        class CompiledButterflyOp
        {
        public:
            CompiledButterflyOp(fft_index_t in0, fft_index_t in1, fft_index_t out,fft_index_t M_index)
                : in0(in0), in1(in1),out(out), M_index(M_index)
            {
                assert(in0 != INVALID_INDEX);
                assert(in1 != INVALID_INDEX);
            }
            void Tick(std::vector<fft_complex_t> &workingMemory)
            {
                fft_complex_t& M = workingMemory[M_index];
                fft_complex_t t1 = workingMemory[in1] * M;
                fft_complex_t t0 = workingMemory[in0];
                workingMemory[out] = t0 + t1;
                workingMemory[out+1] = t0 - t1;
            }
            fft_index_t in0, in1, out, M_index;
#ifndef NDEBUG
            std::string id;
#endif
        };
        struct PlanStep
        {
        public:
            using fft_float_t = double;
            using fft_complex_t = std::complex<double>;

            fft_complex_t Tick(fft_complex_t value, std::vector<fft_complex_t> &workingMemory)
            {
                workingMemory[inputIndex] = value;
                for (std::size_t i = 0; i < ops.size(); ++i)
                {
                    ops[i].Tick(workingMemory);
                }
                return workingMemory[this->outputIndex];
            }
            fft_index_t inputIndex;
            fft_index_t outputIndex;
            std::vector<CompiledButterflyOp> ops;
        };
        class FftPlan
        {
        public:
            struct ConstantEntry
            {
                fft_index_t index;
                fft_complex_t value;
            };
            FftPlan(std::size_t maxDelay, std::size_t storageSize, std::vector<PlanStep> &&ops, std::vector<ConstantEntry> &&constants)
                : norm(fft_float_t(1 / std::sqrt((double)ops.size()))),
                  maxDelay(maxDelay),
                  storageSize(storageSize),
                  steps(std::move(ops)),
                  constants(std::move(constants))
            {
            }
            std::size_t Delay() const { return maxDelay; }
            std::size_t Size() const { return steps.size(); }
            std::size_t StorageSize() const { return storageSize; }
            fft_float_t Norm() const { return norm; }

            fft_complex_t Tick(std::size_t step, fft_complex_t value, std::vector<fft_complex_t> &workingMemory)
            {
                return steps[step].Tick(value*norm, workingMemory);
            }
            void InitializeConstants(std::vector<fft_complex_t> &workingMemory)
            {
                for (const auto &constant : constants)
                {
                    workingMemory[constant.index] = constant.value;
                }
            }
            void PrintPlan();
            void PrintPlan(std::ostream&stream);
            void PrintPlan(const std::string&filename);
            
            void ZeroOutput(size_t output,fft_index_t storageIndex)
            {
                size_t slot = (output + maxDelay) % steps.size();
                steps[slot].outputIndex = storageIndex;
            }
        private:
            double norm;
            std::size_t maxDelay;
            std::size_t storageSize;
            std::vector<PlanStep> steps;
            std::vector<ConstantEntry> constants;
        };
        using plan_ptr = std::shared_ptr<FftPlan>;

    }
    /// @brief Serial DFT that requires the same computational expense for each sample.
    ///
    /// When performing an DFT of an audio stream using a block DFT implementation, the bulk of the comuptational expense occurs
    /// every N samples where N is the size of the FFT. The balanced FFT incurs a fixed computational expense in each sample cycle,
    /// while also making FFT results available earlier.


    class BalancedFft 
    {
    public:
        using Plan = Implementation::FftPlan;
        using plan_ptr = Implementation::plan_ptr;

        BalancedFft(size_t size, FftDirection direction);
        ~BalancedFft() {}
        void PrintPlan();
        void PrintPlan(const std::string& fileName) { plan->PrintPlan(fileName);}
    public:
        size_t Size() const { return plan->Size(); }
        size_t Delay() const { return plan->Delay(); }


        fft_complex_t Tick(fft_complex_t value)
        {
            fft_complex_t result = plan->Tick(planIndex, value, workingMemory);
            if (++planIndex >= plan->Size())
            {
                planIndex = 0;
            }
            return result;
        }
        void Tick(std::size_t frames, fft_float_t *RESTRICT inputs, fft_complex_t *RESTRICT outputs)
        {
            for (size_t i = 0; i < frames; ++i)
            {
                outputs[i] = Tick(inputs[i]);
            }
        }
        void Tick(std::size_t frames, fft_complex_t *RESTRICT inputs, fft_complex_t *RESTRICT outputs)
        {
            for (size_t i = 0; i < frames; ++i)
            {
                outputs[i] = Tick(inputs[i]);
            }
        }
        void Tick(std::size_t frames, fft_complex_t *RESTRICT inputs, fft_float_t *RESTRICT outputs)
        {
            for (size_t i = 0; i < frames; ++i)
            {
                outputs[i] = Tick(inputs[i]).real();
            }
        }
    private:
        void SetPlan(plan_ptr plan);

        std::vector<fft_float_t> inputBuffer;
        std::vector<fft_complex_t> workingMemory;
        plan_ptr plan;
        size_t planIndex = 0;


    private:
        struct PlanKey
        {
            size_t size;
            FftDirection direction;

            bool operator==(const PlanKey &other) const
            {
                return size == other.size && direction == other.direction;
            }
            size_t hash() const
            {
                return (size << 1) ^ (size_t)direction;
            }
        };
        struct PlanKeyHash
        {
            size_t operator()(const PlanKey &key) const
            {
                return key.hash();
            }
        };
        static std::unordered_map<PlanKey, plan_ptr, PlanKeyHash> planCache;
        static plan_ptr GetPlan(std::size_t size, FftDirection direction);
    };

    class BalancedConvolutionSection 
    {
    public:

        using Plan = Implementation::FftPlan;
        using plan_ptr = Implementation::plan_ptr;

        BalancedConvolutionSection(size_t size, size_t offset,std::vector<float> &impulseResponse);

        BalancedConvolutionSection(size_t size, std::vector<float> &impulseResponse)
        : BalancedConvolutionSection(size,0,impulseResponse)
        {

        }
        ~BalancedConvolutionSection() {}

        static size_t GetSectionDelay(size_t size);

        size_t Delay() const { return plan->Delay()-plan->Size()/2; }


        void PrintPlan()
        {
            plan->PrintPlan();
        }
        void PrintPlan(const std::string&fileName)
        {
            plan->PrintPlan(fileName);
        }
        float Tick(float value)
        {
            auto evenResult = plan->Tick(evenPlanIndex, value, evenWorkingMemory).real();
            if (++evenPlanIndex >= plan->Size())
            {
                evenPlanIndex = 0;
            }
            auto oddResult = plan->Tick(oddPlanIndex,value,oddWorkingMemory).real();
            if (++oddPlanIndex >= plan->Size())
            {
                oddPlanIndex = 0;
            }

            return (float)(evenResult+oddResult);
        }
        void Tick(std::size_t frames, float *RESTRICT inputs, float *RESTRICT outputs)
        {
            for (size_t i = 0; i < frames; ++i)
            {
                outputs[i] = Tick(inputs[i]);
            }
        }

    private:
        void SetPlan(plan_ptr plan);

        std::vector<fft_complex_t> evenWorkingMemory;
        std::vector<fft_complex_t> oddWorkingMemory;
        plan_ptr plan;
        size_t evenPlanIndex = 0;
        size_t oddPlanIndex = 0;

        static std::unordered_map<std::size_t, plan_ptr> planCache;
        static plan_ptr GetPlan(std::size_t size, size_t offset,std::vector<float> &data);
    };

    class BalancedConvolution {
    public:
        BalancedConvolution(size_t size, std::vector<float> impulseResponse);

        BalancedConvolution(std::vector<float> impulseResponse)
        :BalancedConvolution(impulseResponse.size(),impulseResponse) {
        }

        float Tick(float value)
        {
            delayLine.push(value);
            double result = 0;
            for (size_t i = 0; i < directConvolutionLength; ++i)
            {
                result += delayLine[i]*(double)directImpulse[i];
            }
            for (size_t i = 0; i < sections.size(); ++i)
            {
                auto&section = sections[i];
                result += section.fftSection.Tick(delayLine[section.sampleDelay]);
            }
            return (float)result;
        }
        void Tick(size_t frames, float*input,float*output)
        {
            for (size_t i = 0; i < frames; ++i)
            {
                output[i] = Tick(input[i]);
            }
        }
        void Tick(std::vector<float>&input, std::vector<float>&output)
        {
            Tick(input.size(),&(input[0]),&(output[0]));
        }

    private:
        class DelayLine {
        public: 
            DelayLine() { SetSize(0);}
            DelayLine(size_t size) {
                SetSize(size);
            }
            void SetSize(size_t size);

            void push(float value)
            {
                head = (head-1) & size_mask;
                storage[head] = value;
            }
            float at(size_t index) const {
                return storage[(head+index) & size_mask];
            }

            float operator[](size_t index) const {
                return at(index);
            }
        private:
            std::vector<float> storage;
            std::size_t head = 0;
            std::size_t size_mask = 0;
        };
    private:
        std::vector<float> directImpulse;
        DelayLine delayLine;
        size_t directConvolutionLength;

        struct Section {
            size_t sampleDelay;
            BalancedConvolutionSection fftSection;
        };

        std::vector<Section> sections;
    };

}
