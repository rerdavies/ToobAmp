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
#include "AudioThreadToBackgroundQueue.hpp"
#include <atomic>
#include "FixedDelay.hpp"
#include "SectionExecutionTrace.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include "../ControlDezipper.h"

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

    inline float UndenormalizeValue(float value)
    {
        return 1.0f + value - 1.0f;
    }

    namespace Implementation
    {
        class AssemblyQueue
        {
            // single-reader, single-writer, designed to be friendly to the reader.
        public:
            AssemblyQueue() { buffer.resize(BUFFER_SIZE); }
            ~AssemblyQueue()
            {
                Close();
            }
            size_t Read(std::vector<float> &inputBuffer, size_t requestedSize)
            {
                std::unique_lock<std::mutex> lock{mutex};

                size_t readIx = 0;
                while (true)
                {
                    if (count != 0)
                    {
                        size_t thisTime = requestedSize;
                        if (count < thisTime)
                            thisTime = count;
                        if (readHead + thisTime > buffer.size())
                        {
                            // split read.
                            size_t firstPart = buffer.size() - readHead;
                            for (size_t i = 0; i < firstPart; ++i)
                            {
                                inputBuffer[readIx++] = buffer[readHead++];
                            }
                            readHead = 0;
                            for (size_t i = firstPart; i < thisTime; ++i)
                            {
                                inputBuffer[readIx++] = buffer[readHead++];
                            }
                        }
                        else
                        {
                            for (size_t i = 0; i < thisTime; ++i)
                            {
                                inputBuffer[readIx++] = buffer[readHead++];
                            }
                            if (readHead >= buffer.size())
                            {
                                readHead = 0;
                            }
                        }
                        count -= thisTime;
                        lock.unlock();
                        write_cv.notify_all();
                        return thisTime;
                    }
                    else if (closed)
                    {
                        for (size_t i = 0; i < requestedSize; ++i)
                        {
                            inputBuffer[i] = 0;
                        }
                        return requestedSize;
                    }

                    read_cv.wait(lock);
                }
            }
            void Close()
            {
                {
                    std::lock_guard<std::mutex> lock{mutex};
                    closed = true;
                }
                read_cv.notify_all();
                write_cv.notify_all();
            }

            void Write(const std::vector<float> &outputBuffer, size_t size)
            {
                std::unique_lock<std::mutex> lock{mutex};
                size_t inputIx = 0;
                while (true)
                {
                    if (closed)
                    {
                        throw DelayLineClosedException();
                    }
                    if (size == 0)
                    {
                        lock.unlock();
                        read_cv.notify_all();
                        return;
                    }
                    
                    if (count < buffer.size())
                    {
                        size_t thisTime = buffer.size() - count;
                        if (thisTime > size)
                        {
                            thisTime = size;
                        }
                        if (thisTime != 0)
                        {
                            if (writeHead + thisTime > buffer.size())
                            {
                                // split write.
                                size_t firstPart = buffer.size() - writeHead;
                                for (size_t i = 0; i < firstPart; ++i)
                                {
                                    buffer[i + writeHead] = outputBuffer[inputIx++];
                                }
                                writeHead = thisTime-firstPart;
                                for (size_t i = 0; i < writeHead; ++i)
                                {
                                    buffer[i] = outputBuffer[inputIx++];
                                }
                            }
                            else
                            {
                                for (size_t i = 0; i < thisTime; ++i)
                                {
                                    buffer[i + writeHead] = outputBuffer[inputIx++];
                                }
                                writeHead += thisTime;
                                if (writeHead >= buffer.size())
                                {
                                    writeHead = 0;
                                }
                            }
                            this->count += thisTime;
                            size -= thisTime;
                        }
                    }
                    else
                    {
                        write_cv.wait(lock);
                    }
                }
            }

        private:
            bool closed = false;
            std::mutex mutex;
            std::condition_variable read_cv;
            std::condition_variable write_cv;
            size_t readHead = 0;
            size_t writeHead = 0;
            static constexpr size_t BUFFER_SIZE = 256;
            size_t count = 0;
            std::vector<float> buffer;
        };
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

        // class ConvolutionAssemblyThread
        // {
        // };

        class DirectConvolutionSection
        {
        public:
            DirectConvolutionSection(
                size_t size,
                size_t sampleOffset, const std::vector<float> &impulseData,
                size_t directSectionDelay = 0,
                size_t inputDelay = 0,
                size_t threadNumber = -1);

            size_t Size() const { return size; }
            size_t SampleOffset() const { return sampleOffset; }
            size_t InputDelay() const { return inputDelay; };
            size_t SectionDelay() const { return sectionDelay; }
            size_t ThreadNumber() const { return threadNumber; }
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

            void Execute(AudioThreadToBackgroundQueue &input, size_t time, LocklessQueue &output);

            bool IsL1Optimized() const
            {
                return fftPlan.IsL1Optimized();
            }
            bool IsL2Optimized() const
            {
                return fftPlan.IsL2Optimized();
            }
            bool IsShuffleOptimized()
            {
                return fftPlan.IsShuffleOptimized();
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
            size_t sectionDelay;
            size_t threadNumber;
            Fft fftPlan;
            size_t size;
            size_t sampleOffset;
            size_t inputDelay;
            std::vector<fft_complex_t> impulseFft;
            size_t bufferIndex;
            std::vector<float> inputBuffer;
            std::vector<fft_complex_t> buffer;
        };
    }

    /// @brief Convolution using a roughly fixed execution time per cycle.
    ///
    /// A convolution section is performed on the audio thread using non-FFT convolution just long enough
    /// to allow FFT convolutions to be performed on background threads.

    class BalancedConvolution : private LocklessQueue::IDelayLineCallback
    {
    public:
        /// @brief Convolution
        /// @param schedulerPolicy Scheduler policy. See remarks.
        /// @param size numer of samples of the impulse response to use.
        /// @param impulseResponse Impulse samples
        /// @param sampleRate Sample rate at which the audio thread will run (NOT the sample rate of the impulse response!)
        /// @param maxAudioBufferSize  Size of the largest value of frames passed to Tick(frames,input,output).
        ///
        /// @remarks
        /// BalancedConvolution uses a set of service threads to calculate large convolution sections. The priority
        /// of the service threads must be below that of audio thread (but still very high). The scheduler policty
        /// determines how the thread priority is set. If Scheduler::Realtime, the worker threads' scheduler policy
        /// is set to SCHED_RR (realtime). Actually priorities are chosen to provide optimal priorities for Linux
        /// audio systems. Very large FFTs are schedule below +6 inorder not to interfere with USB audio services
        /// which run at RT priority +6.
        /// If Scheduler::Normal is specified, the thread priorities are set using nice (3). When running in 
        /// realtime, the schedulerPolicy should always be SchedulerPolicy::Realtime.
        /// SchedulerPolicy::Normal allows unit testing in an environment where the running process may not have
        /// sufficient privileges to set a realtime thread priority, or perhaps processing offline.
        ///
        /// An dedicated thread (the assembly thread) is responsibile for assembling convolution sections calculated on
        /// the background thread into a single stream for consumption by the audio thread. This ensures that (hopefully rare) 
        /// system calls required to wait for background data don't execute on the audio thread. 
        ///
        /// The sampleRate and maxAudioBufferSize parameters are used to control scheduling of the background convolution
        /// sections. Note that sampleRate is the sample rate at which the audio thread is running, NOT the sample
        /// rate of the impulse data (although ideally, the two should be the same). maxAudioBufferSize should be
        /// set to a modest value (e.g. 16..256), since the cost of running balanced convolution sections on the
        /// audio thread is significantly higher than running convolution sections in the background, but the
        /// realtime audio thread mut build up a lead-time significant enough to survive scheduling jitter on
        /// the background audio threads. A high value of maxAudioBufferSize increases the lead-time required,
        /// and consequently, the amount of non-FFT processing that has to be done on the real-time thread.
        ///
        /// If determined to run with large buffer sizes, it would be a good idea to modifiy the realtime processing
        /// thread to use short FFT-sections when processing in realtime. This is not currently implemented. The 
        /// current implementation runs reasonable efficiently with buffer sizes less that 256 frames, and may well
        /// behave badly with buffer sizes of 1024.
        ///
        BalancedConvolution(
            SchedulerPolicy schedulerPolicy,
            size_t size, const std::vector<float> &impulseResponse,
            size_t sampleRate,
            size_t maxAudioBufferSize);

        BalancedConvolution(
            SchedulerPolicy schedulerPolicy,
            const std::vector<float> &impulseResponse,
            size_t sampleRate = 44100,
            size_t maxAudioBufferSize = 128)
            : BalancedConvolution(schedulerPolicy, impulseResponse.size(), impulseResponse, sampleRate, maxAudioBufferSize)
        {
        }

        ~BalancedConvolution();

        size_t GetUnderrunCount() const { return (size_t)underrunCount; }

    private:
        void WaitForAssemblyThreadStartup();
        void SetAssemblyThreadStartupFailed(const std::string & e);
        void SetAssemblyThreadStartupSucceeded();

        std::mutex startup_mutex;
        std::condition_variable startup_cv;
        bool startupSucceeded = false;
        std::string startupError;

        friend class ConvolutionReverb;

        float TickUnsynchronized(float value, float backgroundValue)
        {
            audioThreadToBackgroundQueue.Write(value);

            return (float)(backgroundValue + audioThreadToBackgroundQueue.DirectConvolve(directImpulse));
        }

    public:
        // Highly sub-optimal. Call Tick(size_t,const float*,float*) instead.
        float Tick(float value)
        {
            float output;
            Tick(1, &value, &output);
            return output;
        }
        void Tick(size_t frames, const float * RESTRICT input, float * RESTRICT output)
        {
            size_t ix = 0;
            size_t remaining = frames;
            if (this->directSections.size() == 0)
            {
                for (size_t i = 0; i < frames; ++i)
                {
                    output[ix + i] = TickUnsynchronized(input[ix + i], 0);
                }
            }
            else
            {
                while (remaining != 0)
                {
                    size_t thisTime = 64;
                    if (thisTime > remaining)
                    {
                        thisTime = remaining;
                    }
                    size_t nRead = assemblyQueue.Read(this->assemblyInputBuffer, thisTime);
                    for (size_t i = 0; i < nRead; ++i)
                    {
                        output[ix + i] = TickUnsynchronized(input[ix + i], assemblyInputBuffer[i]);
                    }
                    ix += nRead;
                    remaining -= nRead;
                    audioThreadToBackgroundQueue.SynchWrite();
                }
            }
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

        std::vector<float> assemblyOutputBuffer;
        std::vector<float> assemblyInputBuffer;
        size_t assemblyInputBufferIndex = 0;
        std::unique_ptr<std::thread> assemblyThread;
        Implementation::AssemblyQueue assemblyQueue;
        void AssemblyThreadProc();

        std::atomic<size_t> underrunCount;
        SchedulerPolicy schedulerPolicy;

        size_t GetDirectSectionExecutionTimeInSamples(size_t directSectionSize);
        virtual void OnSynchronizedSingleReaderDelayLineReady();
        virtual void OnSynchronizedSingleReaderDelayLineUnderrun();

        void PrepareSections(size_t size, const std::vector<float> &impulseResponse, size_t sampleRate, size_t maxAudioBufferSize);
        void PrepareThreads();
        class DirectSectionThread;
        DirectSectionThread *GetDirectSectionThread(int threadNumber);

    private:
        using IDelayLineCallback = LocklessQueue::IDelayLineCallback;
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
            bool Execute(AudioThreadToBackgroundQueue &inputDelayLine);

            void Close() { outputDelayLine.Close(); }

            float Tick() { return outputDelayLine.Read(); }
            DirectSection *GetDirectSection() { return this->section; }
            const DirectSection *GetDirectSection() const { return this->section; }

#if EXECUTION_TRACE
        public:
            void SetTraceInfo(SectionExecutionTrace *pTrace, size_t threadNumber)
            {
                this->threadNumber = threadNumber;
                if (section)
                {
                    section->directSection.SetTraceInfo(pTrace, threadNumber);
                }
            }

        private:
            size_t threadNumber = -1;
#endif

        private:
            size_t currentSample = 0;
            LocklessQueue outputDelayLine;
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
            void Execute(AudioThreadToBackgroundQueue &inputDelayLine);
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
        AudioThreadToBackgroundQueue audioThreadToBackgroundQueue;
        size_t directConvolutionLength;

        std::vector<DirectSection> directSections;
    };

    class ConvolutionReverb
    {
    public:
        ConvolutionReverb(SchedulerPolicy schedulerPolicy, size_t size, const std::vector<float> &impulse,size_t sampleRate, size_t maxBufferSize)
            : convolution(schedulerPolicy, size == 0 ? 0 : size - 1, impulse, sampleRate, maxBufferSize) // the last value is recirculated.
        {
            directMixDezipper.To(0, 0);
            reverbMixDezipper.To(1.0, 0);

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
        // float TickUnsynchronizedWithFeedback(float value)
        // {
        //     float recirculationValue = feedbackDelay.Value() * feedbackScale;
        //     float input = Undenormalize(value + recirculationValue);
        //     float reverb = convolution.TickUnsynchronized(input);
        //     feedbackDelay.Put(reverb);

        //     return value * directMix + (reverb)*reverbMix;
        // }

        // float TickUnsynchronizedWithoutFeedback(float value)
        // {

        //     value  = Undenormalize(value);
        //     float reverb = convolution.TickUnsynchronized(value);

        //     return value * directMix + (reverb)*reverbMix;
        // }
    public:
        void SetSampleRate(double rate)
        {
            this->sampleRate = rate;
            this->reverbMixDezipper.SetSampleRate(rate);
            this->directMixDezipper.SetSampleRate(rate);
        }
        void ResetDirectMix(float value)
        {
            this->directMixDezipper.To(value, 0);
        }
        void ResetReverbMix(float value)
        {
            this->reverbMixDezipper.To(value, 0);
        }
        bool IsDezipping()
        {
            return (!reverbMixDezipper.IsComplete()) || (!directMixDezipper.IsComplete());
        }
        void SetDirectMix(float value)
        {
            if (this->sampleRate != 0)
            {
                this->directMixDezipper.To(value, 0.1);
            }
            else
            {
                this->directMixDezipper.To(value, 0);
            }
        }
        void SetReverbMix(float value)
        {
            if (this->sampleRate != 0)
            {
                this->reverbMixDezipper.To(value, 0.1);
            }
            else
            {
                this->reverbMixDezipper.To(value, 0);
            }
        }
        // float Tick(float value)
        // {
        //     float result;
        //     if (hasFeedback)
        //     {
        //         result = TickUnsynchronizedWithFeedback(value);
        //     }
        //     else
        //     {
        //         result = TickUnsynchronizedWithoutFeedback(value);
        //     }
        //     convolution.SynchWrite();
        //     return result;
        // }

        // float TickUnsynchronized(float value)
        // {
        //     float result;
        //     if (hasFeedback)
        //     {
        //         result = TickUnsynchronizedWithFeedback(value);
        //     }
        //     else
        //     {
        //         result = TickUnsynchronizedWithoutFeedback(value);
        //     }
        //     return result;
        // }
        // void TickSynchronize()
        // {
        //     convolution.SynchWrite();
        // }

        void Tick(size_t count, const float  * RESTRICT input, float * RESTRICT output)
        {
            // TODO: there has to be a way to refactor this sensibly. :-/
            if (hasFeedback)
            {
                size_t ix = 0;
                size_t remaining = count;
                if (this->convolution.directSections.size() == 0)
                {
                    // feedback, no direct sections.
                    size_t thisTime = remaining;
                    for (size_t i = 0; i < thisTime; ++i)
                    {
                        float value = input[ix + i];
                        float recirculationValue = feedbackDelay.Value() * feedbackScale;
                        float input = Undenormalize(value + recirculationValue);

                        float reverb = convolution.TickUnsynchronized(input, 0);
                        feedbackDelay.Put(reverb);
                        float returnValue = value * directMixDezipper.Tick() + (reverb)*reverbMixDezipper.Tick();
                        output[ix + i] = returnValue;
                    }
                    ix += thisTime;
                    remaining -= thisTime;
                    convolution.audioThreadToBackgroundQueue.SynchWrite();
                }
                else
                {
                    // feedback, with direct sections.
                    while (remaining != 0)
                    {
                        size_t thisTime = 64;
                        if (thisTime > remaining)
                        {
                            thisTime = remaining;
                        }
                        size_t nRead = convolution.assemblyQueue.Read(convolution.assemblyInputBuffer, thisTime);
                        for (size_t i = 0; i < nRead; ++i)
                        {
                            float value = input[ix + i];
                            float recirculationValue = feedbackDelay.Value() * feedbackScale;
                            float input = Undenormalize(value + recirculationValue);

                            float reverb = convolution.TickUnsynchronized(input, convolution.assemblyInputBuffer[i]);
                            feedbackDelay.Put(reverb);
                            float returnValue = value * directMixDezipper.Tick() + (reverb)*reverbMixDezipper.Tick();
                            output[ix + i] = returnValue;
                        }
                        ix += nRead;
                        remaining -= nRead;
                        convolution.audioThreadToBackgroundQueue.SynchWrite();
                    }
                }
            }
            else
            {
                size_t ix = 0;
                size_t remaining = count;
                if (this->convolution.directSections.size() == 0)
                {
                    // no feedback, no direct sections.
                    size_t thisTime = remaining;
                    for (size_t i = 0; i < thisTime; ++i)
                    {
                        float value = input[ix + i];

                        float reverb = convolution.TickUnsynchronized(value, 0);

                        float returnValue = value * directMixDezipper.Tick() + (reverb)*reverbMixDezipper.Tick();
                        output[ix + i] = returnValue;
                    }
                    ix += thisTime;
                    remaining -= thisTime;
                    convolution.audioThreadToBackgroundQueue.SynchWrite();
                }
                else
                {
                    // no feedback, with direct sections.
                    while (remaining != 0)
                    {
                        size_t thisTime = 64;
                        if (thisTime > remaining)
                        {
                            thisTime = remaining;
                        }
                        size_t nRead = convolution.assemblyQueue.Read(convolution.assemblyInputBuffer, thisTime);
                        for (size_t i = 0; i < nRead; ++i)
                        {
                            float value = input[ix + i];
                            
                            float reverb = convolution.TickUnsynchronized(value, convolution.assemblyInputBuffer[i]);
                            feedbackDelay.Put(reverb);
                            float returnValue = value * directMixDezipper.Tick() + (reverb)*reverbMixDezipper.Tick();
                            output[ix + i] = returnValue;
                        }
                        ix += nRead;
                        remaining -= nRead;
                        convolution.audioThreadToBackgroundQueue.SynchWrite();
                    }
                }
            }
        }
        void Tick(size_t count, const std::vector<float> &input, std::vector<float> &output)
        {
            Tick(count, &input[0], &output[0]);
        }

    private:
        double sampleRate = 0;
        toob::ControlDezipper directMixDezipper;
        toob::ControlDezipper reverbMixDezipper;
        bool hasFeedback = false;
        float feedbackScale = 0;
        FixedDelay feedbackDelay;
        BalancedConvolution convolution;
    };

    /// @brief Enable/display display of section plans
    /// @param enable
    ///
    /// Enable debug tracing of section plans. (Used by ConvolutionReverbTest).

    void SetDisplaySectionPlans(bool enable);

}
