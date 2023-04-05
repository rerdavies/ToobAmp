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
#include <unordered_map>
#include <filesystem>
#include <mutex>
#include "StagedFft.hpp"
#include "BackgroundConvolutionTask.hpp"
#include <atomic>
#include "FixedDelay.hpp"
#include "SectionExecutionTrace.hpp"

#ifndef RESTRICT
#define RESTRICT __restrict // good for MSVC, and GCC.
#endif

namespace LsNumerics
{
    using fft_float_t = double;
    using fft_complex_t = std::complex<fft_float_t>;
    using fft_index_t = int32_t;
    static constexpr fft_index_t CONSTANT_INDEX = fft_index_t(-1);
    static constexpr fft_index_t INVALID_INDEX = fft_index_t(-2);
    class BinaryWriter;
    class BinaryReader;

    enum class FftDirection
    {
        Forward = 1,
        Reverse = -1
    };

    namespace Implementation
    {
        void SlotUsageTest();

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
                head = (head - 1) & sizeMask;
                storage[head] = value;
            }
            float at(size_t index) const
            {
                return storage[(head + index) & sizeMask];
            }

            float operator[](size_t index) const
            {
                return at(index);
            }

        private:
            std::vector<float> storage;
            std::size_t head = 0;
            std::size_t sizeMask = 0;
        };

        class DirectConvolutionSection
        {
        public:
            DirectConvolutionSection(
                size_t size,
                size_t offset, const std::vector<float> &impulseData,
                size_t sectionDelay = 0);

            size_t Size() const { return size; }
            size_t SampleOffset() const { return offset; }
            size_t Delay() const { return schedulerDelay; }
            static size_t GetSectionDelay(size_t size) { return size; }
            float Tick(float input)
            {
                if (bufferIndex >= size)
                {
                    UpdateBuffer();
                }
                // input buffer in reverse order.

                inputBuffer[bufferIndex] = inputBuffer[bufferIndex + size];
                inputBuffer[bufferIndex + size] = input;
                float result = (float(buffer[bufferIndex].real()));
                ++bufferIndex;
                return result;
            }

            void Execute(BackgroundConvolutionTask &input, size_t time, SynchronizedSingleReaderDelayLine &output);

            bool IsL1Optimized() const
            {
                return fftPlan.IsL1Optimized();
            }
            bool IsL2Optimized() const
            {
                return fftPlan.IsL2Optimized();
            }
#if EXECUTION_TRACE
        public:
            void SetTraceInfo(SectionExecutionTrace *pTrace, size_t threadNumber)
            {
                this->pTrace = pTrace;
                this->threadNumber = threadNumber;
            }

        private:
            SectionExecutionTrace *pTrace = nullptr;
#endif

        private:
            using Fft = StagedFft;
            void UpdateBuffer();
            size_t schedulerDelay;
            Fft fftPlan;
            size_t size;
            size_t offset;
            std::vector<fft_complex_t> impulseFft;
            size_t bufferIndex;
            std::vector<float> inputBuffer;
            std::vector<fft_complex_t> buffer;
            size_t threadNumber;
        };
        class CompiledButterflyOp
        {
        public:
            CompiledButterflyOp(fft_index_t in0, fft_index_t in1, fft_index_t out, fft_index_t M_index)
                : in0(in0), in1(in1), out(out), M_index(M_index)
            {
                assert(in0 != INVALID_INDEX);
                assert(in1 != INVALID_INDEX);
            }
            CompiledButterflyOp(BinaryReader &);
            void Tick(std::vector<fft_complex_t> &workingMemory)
            {
                fft_complex_t &M = workingMemory[M_index];
                fft_complex_t t1 = workingMemory[in1] * M;
                fft_complex_t t0 = workingMemory[in0];
                workingMemory[out] = t0 + t1;
                workingMemory[out + 1] = t0 - t1;
            }
            void Tick(fft_complex_t *workingMemory)
            {
                fft_complex_t &M = workingMemory[M_index];
                fft_complex_t t1 = workingMemory[in1] * M;
                fft_complex_t t0 = workingMemory[in0];
                workingMemory[out] = t0 + t1;
                workingMemory[out + 1] = t0 - t1;
            }
            void Write(BinaryWriter &writer) const;
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

            PlanStep() {}
            PlanStep(BinaryReader &reader);

            double Tick(double value, std::vector<fft_complex_t> &workingMemory)
            {
                workingMemory[inputIndex] = value;
                size_t sz = ops.size();
                for (std::size_t i = 0; i < sz; ++i)
                {
                    ops[i].Tick(workingMemory);
                }
                return workingMemory[this->outputIndex].real();
            }

            fft_complex_t Tick(fft_complex_t value, std::vector<fft_complex_t> &workingMemory)
            {
                fft_complex_t *p = &workingMemory[0];
                p[inputIndex] = value;
                for (std::size_t i = 0; i < ops.size(); ++i)
                {
                    ops[i].Tick(workingMemory);
                }
                return p[this->outputIndex];
            }

            void Write(BinaryWriter &writer) const;

            fft_index_t inputIndex;
            fft_index_t inputIndex2;
            fft_index_t outputIndex;
            std::vector<CompiledButterflyOp> ops;
        };
        class FftPlan
        {
        public:
            static const char *MAGIC_FILE_STRING;
            static constexpr uint64_t FILE_VERSION = 101;
            static constexpr uint64_t MAGIC_TAIL_CONSTANT = 0x10394A2BE7F3C34D;

            FftPlan(
                std::size_t maxDelay,
                std::size_t storageSize,
                std::vector<PlanStep> &&ops,
                std::size_t constantsOffset,
                std::vector<fft_complex_t> &&constants,
                size_t startingIndex,
                size_t impulseFftOffset)
                : norm(fft_float_t(1 / std::sqrt((double)ops.size()))),
                  maxDelay(maxDelay),
                  storageSize(storageSize),
                  steps(std::move(ops)),
                  constantsOffset(constantsOffset),
                  constants(std::move(constants)),
                  startingIndex(startingIndex),
                  impulseFftOffset(impulseFftOffset)

            {
            }
            FftPlan(BinaryReader &reader);

            void Write(BinaryWriter &writer) const;

            std::size_t Delay() const { return maxDelay; }
            std::size_t Size() const { return steps.size(); }
            std::size_t StorageSize() const { return storageSize; }
            fft_float_t Norm() const { return norm; }
            std::size_t StartingIndex() const { return startingIndex; }
            std::size_t ImpulseFftOffset() const { return impulseFftOffset; }

            double Tick(std::size_t step, double value, std::vector<fft_complex_t> &workingMemory)
            {
                return steps[step].Tick(value * norm, workingMemory);
            }
            fft_complex_t Tick(std::size_t step, fft_complex_t value, std::vector<fft_complex_t> &workingMemory)
            {
                return steps[step].Tick(value * norm, workingMemory);
            }
            float ConvolutionTick(std::size_t step, float value, std::vector<fft_complex_t> &workingMemory)
            {
                double t = (value * norm);
                auto &planStep = steps[step];
                workingMemory[planStep.inputIndex2] = t; // provide data for the extra inputs.
                float result = (float)planStep.Tick(t, workingMemory);
                return result;
            }
            void InitializeConstants(std::vector<fft_complex_t> &workingMemory)
            {
                for (size_t i = 0; i < constants.size(); ++i)
                {
                    workingMemory[i + constantsOffset] = constants[i];
                }
            }
            void PrintPlan();
            void PrintPlan(std::ostream &stream, bool trimIds);
            void PrintPlan(const std::string &filename);
            void CheckForOverwrites();

            void ZeroOutput(size_t output, fft_index_t storageIndex)
            {
                size_t slot = (output + maxDelay) % steps.size();
                steps[slot].outputIndex = storageIndex;
            }

        private:
            double norm;
            std::size_t maxDelay;
            std::size_t storageSize;
            std::vector<PlanStep> steps;
            std::size_t constantsOffset;
            std::vector<fft_complex_t> constants;
            std::size_t startingIndex;
            std::size_t impulseFftOffset;
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
        void PrintPlan(const std::string &fileName) { plan->PrintPlan(fileName); }

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
        void Reset();

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

    /// @brief Convolution section with balanced exection time per cycle.
    /// For moderate sizes of N, generating the execution plan can take a significant amount of time.
    ///
    /// The recommended way to use this package is to pre-generate files containing the execution plan.
    /// See @ref BalancedConvolution for details.

    class BalancedConvolutionSection
    {
    public:
        using Plan = Implementation::FftPlan;
        using plan_ptr = Implementation::plan_ptr;
        using Fft = StagedFft;

        static void SetPlanFileDirectory(const std::filesystem::path &path)
        {
            planFileDirectory = path;
        }

        BalancedConvolutionSection(size_t size, size_t offset, const std::vector<float> &impulseResponse);

        BalancedConvolutionSection(size_t size, const std::vector<float> &impulseResponse)
            : BalancedConvolutionSection(size, 0, impulseResponse)
        {
        }
        BalancedConvolutionSection(const std::filesystem::path &path,
                                   size_t offset, const std::vector<float> &data);

        void Save(const std::filesystem::path &path);

        ~BalancedConvolutionSection() {}

        static size_t GetSectionDelay(size_t size);

        size_t Size() const { return size; }
        size_t Delay() const { return plan->Delay() - plan->Size() / 2; }

        void PrintPlan()
        {
            plan->PrintPlan();
        }
        void PrintPlan(const std::string &fileName)
        {
            plan->PrintPlan(fileName);
        }
        float Tick(float value)
        {
            auto result = plan->ConvolutionTick(planIndex, value, workingMemory);
            if (++planIndex >= plan->Size())
            {
                planIndex = 0;
            }

            return (float)(result);
        }
        void Tick(std::size_t frames, float *RESTRICT inputs, float *RESTRICT outputs)
        {
            for (size_t i = 0; i < frames; ++i)
            {
                outputs[i] = Tick(inputs[i]);
            }
        }

        void Reset();
        static bool PlanFileExists(size_t size);
        static void ClearPlanCache();

    private:
        static std::mutex planCacheMutex;

        static std::filesystem::path GetPlanFilePath(size_t size);

        void SetPlan(plan_ptr plan, size_t offset, const std::vector<float> &impulseData);

        static std::filesystem::path planFileDirectory;

        size_t size = 0;

        std::vector<fft_complex_t> workingMemory;
        plan_ptr plan;
        size_t planIndex = 0;

        static std::unordered_map<std::size_t, plan_ptr> planCache;
        static plan_ptr GetPlan(std::size_t size);
        static plan_ptr GetPlan(const std::filesystem::path &path);
    };

    /// @brief Convolution using a roughly fixed execution time per cycle.
    ///
    /// Normal convolution requires fft operations that can be enormously expensive every n
    /// cycles. BalancedConvolution spreads out the execution time so that there is roughly
    /// a fixed amount execution time per cycle.
    ///
    /// Generation execution plans can take a significant amount of time (tens or hundreds of seconds).
    /// It is recommended that you configure BalancedConvolution to use pre-generated execution plans
    /// generated at compile time, and stored in files.
    ///
    /// To use pregenerated plan files, follow these steps.
    ///
    /// 1. Generate the execution plans using the GenerateFftPlans executable a build time.
    ///
    ///       GenerateFftPlans <output-directory>
    ///
    /// 2. Copy these files into a fixed location at install time. (May be read-only).
    ///
    /// 3. Call BalancedConvolution::SetPlanFileDirector(<plan-file-directory>) to
    ///    to cause execution plans to be loaded from disk instead of being generated at
    ///    runtime.
    ///
    class BalancedConvolution : private SynchronizedSingleReaderDelayLine::IDelayLineCallback
    {
    public:
        /// @brief Convolution reverb with balanced execution cost per cycle.
        /// @param schedulerPolicy Scheduler policy. See remarks.
        /// @param size numer of samples of the impulse response to use.
        /// @param impulseResponse Impulse samples
        /// @param sampleRate Sample rate at which the audio thread will run (NOT the sample rate of the impulse response!)
        /// @param maxAudioBufferSize  Size of the largest value of frames passed to Tick(frames,input,output).
        ///
        /// @remarks
        /// BalancedConvolution uses a set of service threads to calcualte large convolution sections. The priority
        /// of the service threads must be below that of audio thread (but still very high). The scheduler policty
        /// determines how the thread priority is set. If Scheduler::Realtime, the worker threads' scheduler policy
        /// is set to SCHED_RR (realtime), and the prioriy of the threads are set to basePrioriy+1, basePriority+2,
        /// &c. If Scheduler::Normal, the thread priorities are set using nice (3).
        ///
        /// Note that when running in realtime, the schedulerPolicy should always be SchedulerPolicy::Realtime.
        /// SchedulerPolicy::Normal allows unit testing in an environment where the running process may not have
        /// suffieienctprivileges to set a realtime thread priority.
        ///
        /// The sampleRate and maxAudioBufferSize parameters are used to control scheduling of the background convolution
        /// sections. Note that sampleRate is the sample rate at which the audio thread is running, NOT the sample
        /// rate of the impulse data (although ideally, the two should be the same). maxAudioBufferSize should be
        /// set to a modest value (e.g. 16..256), since the cost of running balanced convolution sections on the
        /// audio thread is significantly higher than running convolution sections in the background, but the
        /// realtime audio thread mut build up a lead-time significant enough to survive scheduling jitter on
        /// the background audio threads. A high value of maxAudioBufferSize increases the lead-time required.
        ///
        BalancedConvolution(
            SchedulerPolicy schedulerPolicy,
            size_t size, const std::vector<float> &impulseResponse,
            size_t sampleRate = 44100,
            size_t maxAudioBufferSize = 128);

        BalancedConvolution(
            SchedulerPolicy schedulerPolicy,
            const std::vector<float> &impulseResponse,
            size_t sampleRate = 44100,
            size_t maxAudioBufferSize = 128)
            : BalancedConvolution(schedulerPolicy, impulseResponse.size(), impulseResponse, sampleRate, maxAudioBufferSize)
        {
        }

        ~BalancedConvolution();
        static void SetPlanFileDirectory(const std::filesystem::path &path)
        {
            BalancedConvolutionSection::SetPlanFileDirectory(path);
        }
        size_t GetUnderrunCount() const { return (size_t)underrunCount; }

        float TickUnsynchronized(float value)
        {
            backgroundConvolutionTask.Write(value);
            double result = 0;
            for (size_t i = 0; i < directConvolutionLength; ++i)
            {
                result += backgroundConvolutionTask[i] * (double)directImpulse[i];
            }
            for (auto &section : balancedSections)
            {
                result += section.fftSection.Tick(backgroundConvolutionTask[section.sampleDelay]);
            }
            for (auto &sectionThread : directSectionThreads)
            {
                result += sectionThread->Tick();
            }
            return (float)result;
        }
        void SynchWrite()
        {
            backgroundConvolutionTask.SynchWrite();
        }

        //[[deprecated( "Use Tick(size_t,float*,float*) instead." )]]
        float Tick(float value)
        {
            float result = TickUnsynchronized(value);
            backgroundConvolutionTask.SynchWrite();
            return result;
        }

        void Tick(size_t frames, float *input, float *output)
        {
            for (size_t i = 0; i < frames; ++i)
            {
                output[i] = TickUnsynchronized(input[i]);
            }
            backgroundConvolutionTask.SynchWrite();
        }
        void Tick(std::vector<float> &input, std::vector<float> &output)
        {
            Tick(input.size(), &(input[0]), &(output[0]));
        }
        void Close();

    private:
#if EXECUTION_TRACE
        SectionExecutionTrace executionTrace;
#endif
        std::atomic<size_t> underrunCount;
        SchedulerPolicy schedulerPolicy;

        virtual void OnSynchronizedSingleReaderDelayLineReady();
        virtual void OnSynchronizedSingleReaderDelayLineUnderrun();

        void PrepareSections(size_t size, const std::vector<float> &impulseResponse, size_t sampleRate, size_t maxAudioBufferSize);
        void PrepareThreads();
        class DirectSectionThread;
        DirectSectionThread *GetDirectSectionThreadBySize(size_t size);

    private:
        using IDelayLineCallback = SynchronizedSingleReaderDelayLine::IDelayLineCallback;
        struct DirectSection
        {
            size_t sampleDelay;
            Implementation::DirectConvolutionSection directSection;
        };

        class ThreadedDirectSection
        {
        public:
            using DirectConvolutionSection = Implementation::DirectConvolutionSection;
            ThreadedDirectSection()
                : section(nullptr)
            {
            }

            void SetWriteReadyCallback(IDelayLineCallback *callback)
            {
                outputDelayLine.SetWriteReadyCallback(callback);
            }
            ThreadedDirectSection(DirectSection &section);

        public:
            size_t Size() const { return section->directSection.Size(); }
            bool Execute(BackgroundConvolutionTask &inputDelayLine);

            void Close() { outputDelayLine.Close(); }

            float Tick() { return outputDelayLine.Read(); }

#if EXECUTION_TRACE
        public:
            void SetTraceInfo(SectionExecutionTrace*pTrace,size_t threadNumber)
            {
                this->threadNumber = threadNumber;
                if (section)
                {
                    section->directSection.SetTraceInfo(pTrace,threadNumber);
                }
            }
        private:
            size_t threadNumber = -1;
#endif

        private:
            size_t currentSample = 0;
            SynchronizedSingleReaderDelayLine outputDelayLine;
            DirectSection *section;
        };
        std::vector<std::unique_ptr<ThreadedDirectSection>> threadedDirectSections;

        class DirectSectionThread
        {
        public:
            DirectSectionThread()
                : threadNumber(-1)
            {
            }
            DirectSectionThread(int threadNumber)
                : threadNumber(threadNumber)
            {
            }
            int GetThreadNumber() const { return threadNumber; }

            float Tick()
            {
                double result = 0;
                for (auto section : sections)
                {
                    result += section->Tick();
                }
                return result;
            }
            void Execute(BackgroundConvolutionTask &inputDelayLine);
            void Close()
            {
                for (auto section : sections)
                {
                    section->Close();
                }
            }
            void AddSection(ThreadedDirectSection *threadedSection)
            {
                sections.push_back(threadedSection);
            }

        private:
            int threadNumber = -1;
            std::vector<ThreadedDirectSection *> sections;
        };

        using section_thread_ptr = std::unique_ptr<DirectSectionThread>;
        std::vector<section_thread_ptr> directSectionThreads;

        static std::mutex globalMutex;

        size_t sampleRate = 48000;
        std::vector<float> directImpulse;
        BackgroundConvolutionTask backgroundConvolutionTask;
        size_t directConvolutionLength;

        struct Section
        {
            size_t sampleDelay;
            BalancedConvolutionSection fftSection;
        };

        std::vector<Section> balancedSections;
        std::vector<DirectSection> directSections;
    };

    class ConvolutionReverb
    {
    public:
        ConvolutionReverb(SchedulerPolicy schedulerPolicy, size_t size, const std::vector<float> &impulse)
            : convolution(schedulerPolicy, size == 0 ? 0 : size - 1, impulse) // the last value is recirculated.
        {
            if (size != 0)
            {
                feedbackScale = impulse[size - 1];
                feedbackDelay.SetSize(size - 1);
                // guard against overflow.
                if (feedbackScale > 0.1)
                    feedbackScale = 0.1;
                if (feedbackScale < -0.1)
                    feedbackScale = -0.1;
            }
            else
            {
                feedbackDelay.SetSize(1);
                feedbackScale = 0;
            }
        }
        void SetFeedback(float feedback, size_t tapPosition)
        {
            feedbackDelay.SetSize(tapPosition);
            feedbackScale = feedback;
            hasFeedback = feedbackScale != 0;
        }

    protected:
        float TickUnsynchronizedWithFeedback(float value)
        {
            float recirculationValue = feedbackDelay.Value() * feedbackScale;

            float reverb = convolution.TickUnsynchronized(value + recirculationValue);
            feedbackDelay.Put(reverb);

            return value * directMix + (reverb)*reverbMix;
        }

        float TickUnsynchronizedWithoutFeedback(float value)
        {

            float reverb = convolution.TickUnsynchronized(value);

            return value * directMix + (reverb)*reverbMix;
        }

    public:
        void SetDirectMix(float value)
        {
            this->directMix = value;
        }
        void SetReverbMix(float value)
        {
            this->reverbMix = value;
        }
        float Tick(float value)
        {
            float result;
            if (hasFeedback)
            {
                result = TickUnsynchronizedWithFeedback(value);
            }
            else
            {
                result = TickUnsynchronizedWithoutFeedback(value);
            }
            convolution.SynchWrite();
            return result;
        }

        void Tick(size_t count, const float *input, float *output)
        {
            if (hasFeedback)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    output[i] = TickUnsynchronizedWithFeedback(input[i]);
                }
            }
            else
            {
                for (size_t i = 0; i < count; ++i)
                {
                    output[i] = TickUnsynchronizedWithoutFeedback(input[i]);
                }
            }
            convolution.SynchWrite();
        }
        void Tick(size_t count, const std::vector<float> &input, std::vector<float> &output)
        {
            Tick(count, &input[0], &output[0]);
        }

    private:
        bool hasFeedback = false;
        float feedbackScale = 0;
        FixedDelay feedbackDelay;
        BalancedConvolution convolution;
        float reverbMix = 1.0;
        float directMix = 0.0;
    };

    /// @brief Enable/display display of section plans
    /// @param enable
    ///
    /// Enable debug tracing of section plans. (Used by ConvolutionReverbTest).

    void SetDisplaySectionPlans(bool enable);

}
