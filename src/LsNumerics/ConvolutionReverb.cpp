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

#include "ConvolutionReverb.hpp"
#include "../ss.hpp"
#include <memory>
#include <cassert>
#include <limits>
#include <iostream>
#include <unordered_set>
#include <stdexcept>
#include "LsMath.hpp"
#include <cstdint>
#include <cstddef>
#include "Fft.hpp"
#include <unordered_map>
#include <fstream>
#include <set>
#include "BinaryWriter.hpp"
#include "BinaryReader.hpp"
#include "../util.hpp"
#include <memory.h>
#include <iostream>

using namespace LsNumerics;

// only generate node IDs in debug mode (very expensive)  (O(n^2)) when enabled.
#define DEBUG_OPS 0
#if DEBUG_OPS
#define SS_ID(x) SS(x)
#else
#define SS_ID(x) ""
#endif

#define RECYCLE_SLOTS 1               // disable for test purposes only
#define DISPLAY_SECTION_ALLOCATIONS 1 // enable for test purposes only

#if DISPLAY_SECTION_ALLOCATIONS
static bool gDisplaySectionPlans = false;
#endif

void LsNumerics::SetDisplaySectionPlans(bool value)
{
#if DISPLAY_SECTION_ALLOCATIONS
    gDisplaySectionPlans = value;
#endif
}

static std::size_t Log2(std::size_t value)
{
    int log = 0;
    while (value > 1)
    {
        ++log;
        value >>= 1;
    }
    return log;
}

static const fft_index_t ToIndex(size_t value)
{
    if (value > (size_t)std::numeric_limits<fft_index_t>::max())
    {
        throw std::logic_error("Maximum index exceeded.");
    }
    return (fft_index_t)(value);
}

// gathered from benchmarks on Raspberry Pi 4.
// aproximate execution time per sample in ns.
struct ExecutionEntry
{
    size_t n;
    double microsecondsPerExecution;
    int threadNumber;
    int schedulingOffset = 0;
};

constexpr int INVALID_THREAD_ID = -1; // DirectionSections with this size are not encounered in normal use.

std::mutex BalancedConvolution::globalMutex;

static std::vector<ExecutionEntry> executionTimePerSampleNs{
    // Impossible, or directly executed.
    {0, 0, INVALID_THREAD_ID},
    {1, 0, INVALID_THREAD_ID},
    {2, 0, INVALID_THREAD_ID},
    {4, 82.402, INVALID_THREAD_ID},
    {8, 75.522, INVALID_THREAD_ID},
    {16, 78.877, INVALID_THREAD_ID},
    {32, 86.127, INVALID_THREAD_ID},
    {64, 92.286, INVALID_THREAD_ID},

    // executed on thread 1.
    {128, 244, 1, 0}, // execution times in microseconds measured under real-time conditions (with write barrier)
    {256, 244, 1, 0},
    {512, 368, 1, 0},
    {1024, 594, 2, 0},
    // executed on thread 2
    {2048, 977, 2, 0},
    {4096, 2093, 2, 0},
    {8192, 3662, 3, 0},

    {
        16384,
        15174,
        3,
    },
    {32768, 36324, 4},
    {65536, 60926, 5},
    {131072, 60926 * 2.2, 6},
    {262144, 60926 * 2.2 * 2.2, 7},
    {524288, 60926 * 2.2 * 2.2 * 2.2, 8},

};

constexpr int MAX_THREAD_ID = 11;

constexpr size_t INVALID_EXECUTION_TIME = std::numeric_limits<size_t>::max();

static int GetDirectSectionThreadId(size_t size)
{
    for (auto &entry : executionTimePerSampleNs)
    {
        if (entry.n == size)
        {
            return entry.threadNumber;
        }
    }
    return INVALID_THREAD_ID;
}
std::vector<size_t> directSectionLeadTimes;

size_t BalancedConvolution::GetDirectSectionExecutionTimeInSamples(size_t directSectionSize)
{
    for (const auto &entry : executionTimePerSampleNs)
    {
        if (entry.n == directSectionSize)
        {
            return (size_t)std::ceil(entry.n * 1E-6 * sampleRate);
        }
    }
    throw std::invalid_argument("invalid directSectionSize.");
}

static void UpdateDirectExecutionLeadTimes(size_t sampleRate, size_t maxAudioBufferSize)
{
    // calculate the lead time in samples based on how long it takes to execute
    // a direction section of a particular size.

    // Calculate per-thread worst execution times.
    // (Service threads handle groups of block sizes.)
    std::vector<int> pooledExecutionTime;
    pooledExecutionTime.resize(MAX_THREAD_ID + 1);
    for (const auto &entry : executionTimePerSampleNs)
    {
        if (entry.threadNumber != INVALID_THREAD_ID)
        {
            double executionTimeSeconds = entry.microsecondsPerExecution * 1E-6;
            executionTimeSeconds *= ((double)sampleRate) / 48000; // benchmarks were for 48000.
            executionTimeSeconds *= 1.8 / 1.5;                    // in case we're running on 1.5Ghz pi.
            executionTimeSeconds *= 2;                            // because there may be duplicates
            size_t samplesLeadTime = (std::ceil(executionTimeSeconds * sampleRate));

            pooledExecutionTime[entry.threadNumber] += samplesLeadTime;
        }
    }

    // std::cout << "Thread pool execution times" << std::endl;
    // for (size_t i = 0; i < pooledExecutionTime.size(); ++i)
    // {
    //      std::cout << std::setw(12) << std::right << i << std::setw(12) << std::right << pooledExecutionTime[i] << std::endl;
    // }

    // std::cout <<  std::endl;
    directSectionLeadTimes.resize(executionTimePerSampleNs.size());
    for (size_t i = 0; i < directSectionLeadTimes.size(); ++i)
    {
        directSectionLeadTimes[i] = INVALID_EXECUTION_TIME;
    }
    double schedulingJitterSeconds = 0.002; // 2ms for scheduiling overhead.

    size_t schedulingJitter = (size_t)(schedulingJitterSeconds * sampleRate + maxAudioBufferSize);

    for (const auto &entry : executionTimePerSampleNs)
    {
        size_t log2N = Log2(entry.n);
        if (entry.threadNumber != INVALID_THREAD_ID)
        {
            directSectionLeadTimes[log2N] = pooledExecutionTime[entry.threadNumber] + schedulingJitter + entry.n;
        }
    }

    // std::cout << "Direct Section Lead Times" << std::endl;
    // for (size_t i = 0; i < directSectionLeadTimes.size(); ++i)
    // {
    //     if (directSectionLeadTimes[i] != INVALID_EXECUTION_TIME)
    //     {
    //         std::cout << std::setw(12) << std::right << (1 << i) << std::setw(12) << std::right << directSectionLeadTimes[i] << std::endl;
    //     }
    // }
}
static size_t GetDirectSectionLeadTime(size_t directSectionSize)
{
    size_t log2Size = Log2(directSectionSize);
    if (log2Size >= directSectionLeadTimes.size())
    {
        throw std::logic_error("Unexpected direct section lead time.");
    }
    auto result = directSectionLeadTimes[Log2(directSectionSize)];
    if (result == INVALID_EXECUTION_TIME)
    {
        throw std::logic_error("Unexpected direct section lead time.");
    }
    return result;
}

std::string MaxString(const std::string &s, size_t maxLen)
{
    if (s.length() < maxLen)
    {
        return s;
    }
    return s.substr(0, maxLen - 3) + "...";
}
// When enabled, reduces workingMemory size as much as possible
// by re-using slots in workingMemory whenever possible.
namespace LsNumerics::Implementation
{
    class SlotUsage
    {
    private:
        fft_index_t planSize;

        struct UsageEntry
        {
            fft_index_t from;
            fft_index_t to;
        };
        std::vector<UsageEntry> used;

    public:
        SlotUsage()
        {
        }
        SlotUsage(size_t planSize)
        {
            SetPlanSize(planSize);
        }
        void SetPlanSize(size_t planSize)
        {
            this->planSize = ToIndex(planSize);
        }
        size_t Size() { return used.size(); }
        // range: [from,to)
        void Add(fft_index_t from, fft_index_t to)
        {
            if (from >= planSize)
            {
                from -= planSize;
                to -= planSize;
            }
            else if (to > planSize)
            {
                to -= planSize;
                Add(0, to);
                Add(from, planSize);
                return;
            }
            auto addIndex = used.end();
            for (auto i = used.begin(); i != used.end(); ++i)
            {
                if ((*i).from >= from)
                {
                    addIndex = i;
                    break;
                }
                if ((*i).to == from)
                {
                    (*i).to = to;
                    return;
                }
                if ((*i).from > to)
                {
                    throw std::logic_error("Overlapping range.");
                }
            }
            UsageEntry entry{from, to};
            if (addIndex != used.end() && entry.to >= (addIndex)->from)
            {
                if (entry.to == (addIndex)->from)
                {
                    entry.to = (*addIndex).to;
                    *addIndex = entry;
                    return;
                }
                if (addIndex->to == addIndex->from && entry.from == addIndex->from)
                {
                    *addIndex = entry;
                    return;
                }
                throw std::logic_error("Overlapping range.");
            }
            else
            {
                used.insert(addIndex, entry);
            }
        }
        bool contains(fft_index_t time)
        {
            return contains_any(time, time + 1);
        }
        bool contains_any(fft_index_t from, fft_index_t to)
        {
            if (from >= planSize)
            {
                if (from == to)
                {
                    to -= planSize;
                }
                from -= planSize;
            }
            if (to > planSize)
            {
                to -= planSize;
            }
            if (from > to)
            {
                if (contains_any(0, to))
                    return true;
                return contains_any(from, planSize);
            }
            if (used.size() == 0)
                return false;

            // binary search to find the first entry that where entry.from >= from
            ptrdiff_t minIndex = 0;
            ptrdiff_t maxIndex = used.size() - 1;
            while (minIndex < maxIndex)
            {
                size_t mid = (minIndex + maxIndex) / 2;
                auto &entry = used[mid];
                if (entry.from == from)
                {
                    minIndex = maxIndex = mid;
                }
                else if (entry.from > to)
                {
                    maxIndex = mid - 1;
                }
                else /* entry.from < to*/
                {
                    if (entry.to > from)
                    {
                        minIndex = maxIndex = mid;
                    }
                    else
                    {
                        minIndex = mid + 1;
                    }
                }
            }
            if (minIndex < 0 || minIndex >= (ptrdiff_t)used.size())
                return false;
            auto &entry = used[minIndex];
            if (from == to) // a temporary borrow may not overwrite existing data.
            {
                if (entry.from == entry.to)
                {
                    return false;
                }
                return from < entry.to && to + 1 > entry.from;
            }
            else
            {
                if (entry.to == entry.from)
                {
                    if (from == to && from == entry.from)
                    {
                        return false;
                    }
                    if (from < entry.to + 1 && to > entry.from)
                    {
                        return true;
                    }
                }
                if (from < entry.to && to > entry.from)
                {
                    return true;
                }
            }
            return false;
        }
        void Print(std::ostream &o) const
        {
            o << '[';
            for (const auto &entry : used)
            {
                o << '(' << entry.from << ',' << entry.to << ')';
            }
            o << ']';
        }
        void Print() const
        {
            Print(std::cout);
            std::cout << std::endl;
        }
        std::string ToString() const;
    };

    std::string SlotUsage::ToString() const
    {
        std::stringstream s;
        Print(s);
        return s.str();
    }
    std::ostream &operator<<(std::ostream &o, const SlotUsage &slotUsage)
    {
        slotUsage.Print(o);
        return o;
    }

    class FftOp;

    class IndexAllocator
    {

    public:
        std::size_t recycledSlots = 0;
        std::size_t discardedSlots = 0;

        IndexAllocator(size_t planSize)
        {
            this->planSize = planSize;
        }
        std::vector<SlotUsage> slotUsages;

        fft_index_t Allocate(std::size_t entries, FftOp *op);
        void Free(fft_index_t index, std::size_t size, FftOp *op);

    private:
        SlotUsage &GetSlotUsage(size_t index)
        {
            size_t size = slotUsages.size();
            if (index >= size)
            {
                size_t newSize = slotUsages.size();
                if (newSize < (size_t)(this->planSize))
                {
                    newSize = this->planSize * 2;
                }
                while (newSize <= index)
                {
                    newSize *= 2;
                }
                slotUsages.resize(newSize);
                for (size_t i = size; i < newSize; ++i)
                {
                    slotUsages[i].SetPlanSize(this->planSize);
                }
            }
            return slotUsages[index];
        }

        struct FreeIndexEntry
        {
            fft_index_t index;
        };
        std::vector<FreeIndexEntry> freeIndices;
        fft_index_t nextIndex = 0;
        fft_index_t planSize;
    };

    class FftOp
    {
    public:
        enum class OpType
        {
            InputOp,
            ConstantOp,
            ButterflyOp,
            LeftOutput,
            RightOutput

        };

        using op_ptr = std::shared_ptr<FftOp>;

        FftOp(OpType opType)
            : opType(opType)
        {
        }
        virtual ~FftOp()
        {
            for (auto &input : inputs)
            {
                input->RemoveOutput(this);
            }
        }
        virtual std::string Id() const = 0;

        virtual void AllocateMemory(IndexAllocator &allocator) = 0;
        virtual void FreeInputReferences(IndexAllocator &allocator) {}
        virtual void FreeStorageReference(IndexAllocator &allocator, FftOp *op) {}
        virtual void AddInputReference() {}

        void AddInput(op_ptr op)
        {
            assert(op);
            this->inputs.push_back(op);
            op->outputs.push_back(this);
            fft_index_t inputT = op->GetEarliestAvailable();
            assert(inputT != INVALID_INDEX);
            if (inputT > earliest)
            {
                earliest = inputT;
            }
        }
        void RemoveOutput(FftOp *output)
        {
            for (auto i = outputs.begin(); i != outputs.end(); ++i)
            {
                if ((*i) == output)
                {
                    outputs.erase(i);
                    return;
                }
            }
            throw std::logic_error("Output list corrupted.");
        }
        bool GetReady() const { return ready; }
        void SetReady(bool value = true) { ready = value; }
        OpType GetOpType() const { return opType; }
        const std::vector<op_ptr> &GetInputs() const
        {
            return inputs;
        }
        FftOp *GetInput(std::size_t index) const
        {
            return inputs[index].get();
        }
        const std::vector<FftOp *> &GetOutputs() const
        {
            return outputs;
        }
        FftOp *GetOutput(size_t index)
        {
            return outputs[index];
        }
        const FftOp *GetOutput(size_t index) const
        {
            return outputs[index];
        }

        virtual fft_index_t GetLatestUse() const
        {
            fft_index_t result = GetEarliestAvailable();
            for (auto output : outputs)
            {
                fft_index_t t;
                switch (output->GetOpType())
                {
                case OpType::RightOutput:
                case OpType::LeftOutput:
                    t = output->GetLatestUse();
                    break;
                default:
                    t = output->GetEarliestAvailable();
                    break;
                }
                if (t > result)
                {
                    result = t;
                }
            }
            return result;
        }

        static void GetOps(std::set<FftOp *> &set, FftOp *op)
        {
            if (set.find(op) != set.end())
                return;
            if (op->GetOpType() == FftOp::OpType::ButterflyOp)
            {
                set.insert(op);
            }
            for (auto &input : op->GetInputs())
            {
                input->GetOps(set, input.get());
            }
        }
        static size_t GetTotalOps(std::vector<FftOp::op_ptr> &outputs)
        {
            std::set<FftOp *> set;
            for (auto &output : outputs)
            {
                GetOps(set, output.get());
            }
            return set.size();
        }

        virtual fft_index_t GetEarliestAvailable() const { return earliest; }

        void SetEarliestAvailable(fft_index_t time)
        {
            if (time > this->earliest)
            {
                this->earliest = time;
                // for (auto output : outputs)
                // {
                //     output->SetEarliestAvailable(time);
                // }
            }
        }
        void UpdateEarliestAvailable()
        {
            fft_index_t result = this->earliest;
            for (auto &input : inputs)
            {
                fft_index_t inputTime = input->GetEarliestAvailable();

                if (inputTime > result)
                {
                    result = inputTime;
                }
            }
            this->earliest = result;
        }
        virtual fft_index_t GetStorageIndex() const
        {
            return storage_index;
        }
        void SetStorageIndex(fft_index_t index)
        {
            storage_index = index;
        }
        bool HasStorageIndex() const
        {
            return GetStorageIndex() != INVALID_INDEX;
        }

    private:
        bool ready = false;
        fft_index_t storage_index = INVALID_INDEX;
        fft_index_t earliest = 0;
        std::vector<op_ptr> inputs;
        std::vector<FftOp *> outputs;

        OpType opType;
    };

    class InputOp : public FftOp
    {
    public:
        using base = FftOp;
        InputOp(std::size_t t, std::size_t planSize)
            : FftOp(OpType::InputOp),
              t((fft_index_t)t)
        {
            SetEarliestAvailable(this->t);
            SetStorageIndex(this->t);
        }
        virtual fft_index_t GetEarliestAvailable() const
        {
            return t;
        }

        virtual void AllocateMemory(IndexAllocator &allocator)
        {
        }
        virtual std::string Id() const
        {
            return SS_ID("x[" << t << ']');
        }
        fft_index_t GetT() const { return t; }

    private:
        fft_index_t t;
    };
    class ConstantOp : public FftOp
    {
    public:
        using base = FftOp;
        ConstantOp(const fft_complex_t &value)
            : FftOp(OpType::ConstantOp),
              value(value)
        {
            SetEarliestAvailable(-1);
        }
        fft_complex_t GetValue() const { return value; }

        virtual fft_index_t GetEarliestAvailable() const
        {
            return CONSTANT_INDEX;
        }

        virtual void AllocateMemory(IndexAllocator &allocator)
        {
            if (GetStorageIndex() == INVALID_INDEX)
                SetStorageIndex(allocator.Allocate(2, this));
        }
        virtual std::string Id() const
        {
            return SS_ID("k[" << value << ']');
        }

    private:
        fft_complex_t value;
    };

    class LeftOutputOp : public FftOp
    {
    public:
        LeftOutputOp(op_ptr op)
            : FftOp(OpType::LeftOutput)
        {
            assert(op->GetOpType() == FftOp::OpType::ButterflyOp);
            AddInput(op);
        }
        virtual fft_index_t GetStorageIndex() const
        {
            return GetInput(0)->GetStorageIndex();
        }
        virtual void AllocateMemory(IndexAllocator &allocator)
        {
            if (!HasStorageIndex())
            {
                auto parent = GetInput(0);
                assert(parent->GetOpType() == FftOp::OpType::ButterflyOp);
                parent->AllocateMemory(allocator);
                SetStorageIndex(parent->GetStorageIndex());
            }
        }
        virtual fft_index_t GetLatestUse() const
        {
            const auto &outputs = this->GetOutputs();
            if (outputs.size() == 0)
                return GetEarliestAvailable();
            return GetOutput(0)->GetEarliestAvailable();
        }
        virtual fft_index_t GetEarliestAvailable() const
        {
            return GetInput(0)->GetEarliestAvailable();
        }

        virtual void FreeStorageReference(IndexAllocator &allocator, FftOp *op)
        {
            GetInput(0)->FreeStorageReference(allocator, op);
        }
        virtual void AddInputReference()
        {
            GetInput(0)->AddInputReference();
        }

        virtual std::string Id() const
        {
            return SS_ID(GetInputs()[0]->Id() << ".L");
        }
    };
    class RightOutputOp : public FftOp
    {
    public:
        RightOutputOp(op_ptr op)
            : FftOp(OpType::RightOutput)
        {
            AddInput(op);
        }

        virtual std::string Id() const
        {
            return SS_ID(GetInputs()[0]->Id() << ".R");
        }
        virtual fft_index_t GetStorageIndex() const
        {
            return GetInput(0)->GetStorageIndex() + 1;
        }
        virtual void AllocateMemory(IndexAllocator &allocator)
        {
            if (!HasStorageIndex())
            {
                auto parent = GetInput(0);
                assert(parent->GetOpType() == FftOp::OpType::ButterflyOp);
                parent->AllocateMemory(allocator);
                SetStorageIndex(parent->GetStorageIndex() + 1);
            }
        }
        virtual fft_index_t GetEarliestAvailable() const
        {
            return GetInput(0)->GetEarliestAvailable();
        }
        virtual fft_index_t GetLatestUse() const
        {
            const auto &outputs = GetOutputs();
            if (outputs.size() == 0) // eg an output node.
            {
                return this->GetEarliestAvailable();
            }
            return GetOutput(0)->GetEarliestAvailable();
        }

        virtual void FreeStorageReference(IndexAllocator &allocator, FftOp *op)
        {
            GetInput(0)->FreeStorageReference(allocator, op);
        }
        virtual void AddInputReference()
        {
            GetInput(0)->AddInputReference();
        }
    };

    class ButterflyOp : public FftOp
    {
    public:
        ButterflyOp(op_ptr in0, op_ptr in1, op_ptr mConstant)
            : FftOp(OpType::ButterflyOp)
        {
            AddInput(in0);
            AddInput(in1);
            AddInput(mConstant);
        }
        std::string Id() const
        {
            ConstantOp *mConstant = (ConstantOp *)GetInput(2);
            (void)mConstant; // mark as used.
            return SS_ID("bf(" << GetInput(0)->Id() << "," << GetInput(1)->Id() << "," << mConstant->GetValue());
        }
        FftOp *GetM() const { return GetInput(2); }
        virtual void AllocateMemory(IndexAllocator &allocator)
        {
            if (!HasStorageIndex())
            {
                SetStorageIndex(allocator.Allocate(2, this));
                references += 2;
            }
        }
        virtual void AddInputReference()
        {
            ++references;
        }
        virtual void FreeInputReferences(IndexAllocator &allocator)
        {
            GetInput(0)->FreeStorageReference(allocator, this);
            GetInput(1)->FreeStorageReference(allocator, this);
        }
        virtual void FreeStorageReference(IndexAllocator &allocator, FftOp *op)
        {
            assert(references > 0);
            if (--references == 0)
            {
                allocator.Free(GetStorageIndex(), 2, this);
            }
        }

    private:
        int references = 0;
    };

    void IndexAllocator::Free(fft_index_t index, std::size_t size, FftOp *op)
    {
#if RECYCLE_SLOTS
        if (size == 2 && op != nullptr)
        {
            // std::cout << "Free: " << index << "  from: " << currentTime << " to: " << expiryTime << " " << usage << std::endl;
            fft_index_t currentTime = op->GetEarliestAvailable();

            fft_index_t expiryTime = op->GetLatestUse();
            auto &usage = GetSlotUsage(index);
            if (usage.Size() >= 100) // prevent O(N^2) behaviour for large FFTs.
            {
                ++this->discardedSlots;
                // don't recycle this slot again.
            }
            else
            {
                usage.Add(currentTime, expiryTime);

                freeIndices.push_back(FreeIndexEntry{index});
            }
        }
#else
        // no recycling of slots.
#endif
    }
    fft_index_t IndexAllocator::Allocate(std::size_t entries, FftOp *op)
    {

#if RECYCLE_SLOTS

        if (entries == 2 && op != nullptr && freeIndices.size() != 0)
        {
            fft_index_t currentTime = op->GetEarliestAvailable();
            fft_index_t expiryTime = op->GetLatestUse();

            for (ptrdiff_t i = freeIndices.size() - 1; i >= 0; --i)
            {
                auto &entry = freeIndices[i];
                auto &usage = GetSlotUsage(entry.index);
                usage.SetPlanSize(this->planSize);
                if (!usage.contains_any(currentTime, expiryTime))
                {
                    auto result = entry.index;
                    // std::cout << "Allocate: time: " << currentTime << " index: " << entry.index
                    //     << " [" << currentTime << "," << expiryTime << ")" << " "
                    //     << " [" << (currentTime % this->planSize) << "," << (expiryTime % this->planSize) << ")" << " "
                    //     << usage << std::endl;
                    // std::cout << "  " << MaxString(op->Id(),60) << std::endl;
                    freeIndices.erase(freeIndices.begin() + i);
                    recycledSlots++;
                    return result;
                }
            }
        }
#endif
        fft_index_t result = nextIndex;

        nextIndex += entries;
        return result;
    }
}

using namespace LsNumerics::Implementation;

static size_t convolutionSampleRate = (size_t)-1;
static size_t convolutionMaxAudioBufferSize = (size_t)-1;

BalancedConvolution::BalancedConvolution(SchedulerPolicy schedulerPolicy, size_t size, const std::vector<float> &impulseResponse, size_t sampleRate, size_t maxAudioBufferSize)
    : schedulerPolicy(schedulerPolicy), isStereo(false), assemblyQueue(false)
{
    this->assemblyInputBuffer.resize(1024);
    this->assemblyOutputBuffer.resize(1024);
    PrepareSections(size, impulseResponse, nullptr, sampleRate, maxAudioBufferSize);
    PrepareThreads();
}

BalancedConvolution::BalancedConvolution(
    SchedulerPolicy schedulerPolicy,
    size_t size,
    const std::vector<float> &impulseResponseLeft, const std::vector<float> &impulseResponseRight,
    size_t sampleRate,
    size_t maxAudioBufferSize)
    : schedulerPolicy(schedulerPolicy), isStereo(true), assemblyQueue(true)
{

    this->assemblyInputBuffer.resize(1024);
    this->assemblyOutputBuffer.resize(1024);
    this->assemblyInputBufferRight.resize(1024);
    this->assemblyOutputBufferRight.resize(1024);
    PrepareSections(size, impulseResponseLeft, &impulseResponseRight, sampleRate, maxAudioBufferSize);
    PrepareThreads();
}

BalancedConvolution::DirectSectionThread *BalancedConvolution::GetDirectSectionThread(int threadNumber)
{
    for (auto &thread : directSectionThreads)
    {
        if (thread->GetThreadNumber() == threadNumber)
        {
            return thread.get();
        }
    }
    directSectionThreads.emplace_back(std::make_unique<DirectSectionThread>(threadNumber));
    return directSectionThreads[directSectionThreads.size() - 1].get();
}
void BalancedConvolution::PrepareThreads()
{
    threadedDirectSections.reserve(directSections.size());
    for (size_t i = 0; i < directSections.size(); ++i)
    {
        DirectSection &section = directSections[i];
        threadedDirectSections.emplace_back(std::make_unique<ThreadedDirectSection>(section));
    }

    for (auto &threadedDirectSection : threadedDirectSections)
    {
        auto sectionThread = GetDirectSectionThread(threadedDirectSection->GetDirectSection()->directSection.ThreadNumber());
        sectionThread->AddSection(threadedDirectSection.get());
#if EXECUTION_TRACE
        threadedDirectSection->SetTraceInfo(&executionTrace, sectionThread->GetThreadNumber());
#endif
        threadedDirectSection->SetWriteReadyCallback(dynamic_cast<IDelayLineCallback *>(this));
    }

    for (size_t i = 0; i < directSectionThreads.size(); ++i)
    {
        DirectSectionThread *thread = directSectionThreads[i].get();
        if (thread)
        {
            audioThreadToBackgroundQueue.CreateThread(
                [this, thread]()
                {
                    thread->Execute(this->audioThreadToBackgroundQueue);
                },
                (int)thread->GetThreadNumber());
        }
    }
    if (this->directSectionThreads.size() != 0)
    {
        this->assemblyThread = std::make_unique<std::thread>(std::bind(&BalancedConvolution::AssemblyThreadProc, this));
        this->WaitForAssemblyThreadStartup();
    }
}
void BalancedConvolution::PrepareSections(size_t size, const std::vector<float> &impulseResponse, const std::vector<float> *impulseResponseRight, size_t sampleRate, size_t maxAudioBufferSize)
{
    constexpr size_t INITIAL_SECTION_SIZE = 128;
    constexpr size_t INITIAL_DIRECT_SECTION_SIZE = 128;

    // nb: global data, but constructor is always protected by the cache mutex.
    {
        std::lock_guard lock{globalMutex};
        if (convolutionSampleRate != sampleRate || convolutionMaxAudioBufferSize != maxAudioBufferSize)
        {
            convolutionSampleRate = sampleRate;
            convolutionMaxAudioBufferSize = maxAudioBufferSize;
            UpdateDirectExecutionLeadTimes(sampleRate, maxAudioBufferSize);
        }
    }
    int stereoScaling = isStereo ? 2 : 1;

    size_t delaySize = -1;
    if (size < INITIAL_SECTION_SIZE)
    {
        directConvolutionLength = size;
        delaySize = directConvolutionLength;
    }
    else
    {

        size_t directSectionSize = INITIAL_DIRECT_SECTION_SIZE;

        directConvolutionLength = GetDirectSectionLeadTime(directSectionSize) * stereoScaling;

        if (directConvolutionLength > size)
        {
            directConvolutionLength = size;
        }
        delaySize = directConvolutionLength;

        size_t sampleOffset = directConvolutionLength;

        directSections.reserve(16);

        int threadNumber = 0;
        size_t executionOffsetInSamples = 0;

        while (sampleOffset < size)
        {

            size_t remaining = size - sampleOffset;

            size_t directSectionDelay;

            // Pick a candidate Direct section.
            while (true)
            {
                directSectionDelay = GetDirectSectionLeadTime(directSectionSize) * stereoScaling + executionOffsetInSamples;
                if (directSectionDelay == std::numeric_limits<size_t>::max())
                {
                    throw std::logic_error("Failed to schedule direct section.");
                }
                if (directSectionDelay > sampleOffset)
                {
                    throw std::logic_error("Convolution scheduling failed.");
                }

                // don't increase direct section size if we can reach the end with the current size.
                if (directSectionSize >= remaining)
                {
                    break;
                }
                // don't increase the direct section size if we don't have enough samples.
                size_t nextDirectSectionDelay = GetDirectSectionLeadTime(directSectionSize * 2) * stereoScaling + executionOffsetInSamples;
                if (nextDirectSectionDelay > sampleOffset)
                {
                    break;
                }
                directSectionSize = directSectionSize * 2;
            }

            // If remaining samples are less that half of the directSection size, reduce the size of the direct section.
            while (remaining <= directSectionSize / 2 && directSectionSize > INITIAL_SECTION_SIZE)
            {
                directSectionSize = directSectionSize / 2;
                directSectionDelay = GetDirectSectionLeadTime(directSectionSize) + executionOffsetInSamples;
            }

            {

                size_t inputDelay = executionOffsetInSamples & (directSectionSize - 1);

                if (inputDelay > sampleOffset - directSectionDelay)
                {
                    inputDelay = ((sampleOffset - directSectionDelay) * 2 / 3) & (directSectionSize - 1); // just do what we can. Effectively, a random placement.
                }

#if DISPLAY_SECTION_ALLOCATIONS
                if (gDisplaySectionPlans)
                {
                    std::cout << "direct   "
                              << "sampleOffset: " << sampleOffset
                              << " SectionSize: " << directSectionSize
                              << " sectionDelay: " << directSectionDelay
                              << " input delay: " << inputDelay
                              << std::endl;
                }
#endif

                size_t myDelaySize = sampleOffset + directSectionSize + 256; // long enough to survive an underrun.
                if (myDelaySize > delaySize)
                {
                    delaySize = myDelaySize;
                }

                // sections get their assigned thread number, except for the size-reduced last section, which goes on the same thread as
                // its predecessor.

                int t = GetDirectSectionThreadId(directSectionSize);
                if (t > threadNumber)
                {
                    threadNumber = t;
                }

                directSections.emplace_back(
                    DirectSection{
                        inputDelay,
                        DirectConvolutionSection(
                            directSectionSize,
                            sampleOffset,
                            impulseResponse,
                            impulseResponseRight,
                            directSectionDelay,
                            inputDelay,
                            threadNumber)});
                sampleOffset += directSectionSize;
                executionOffsetInSamples += this->GetDirectSectionExecutionTimeInSamples(directSectionSize);
            }
        }
    }

    // Separate the portion of the impulse that's calculated directly (without FFT) on the audio thread.
    // Note that the order of samples is reversed here, to simplify realtime calculations.
    directImpulse.resize(directConvolutionLength);
    for (size_t i = 0; i < directConvolutionLength; ++i)
    {
        directImpulse[directConvolutionLength - 1 - i] = i < impulseResponse.size() ? impulseResponse[i] : 0;
    }
    if (isStereo)
    {
        directImpulseRight.resize(directConvolutionLength);
        for (size_t i = 0; i < directConvolutionLength; ++i)
        {
            directImpulseRight[directConvolutionLength - 1 - i] = i < (*impulseResponseRight).size() ? ((*impulseResponseRight)[i]) : 0;
        }
    }
    audioThreadToBackgroundQueue.SetSize(delaySize + 1, 256, this->schedulerPolicy, isStereo);
}
static int NextPowerOf2(size_t value)
{
    size_t result = 1;
    while (result < value)
    {
        result *= 2;
    }
    return result;
}

void Implementation::DelayLine::SetSize(size_t size)
{
    size = NextPowerOf2(size);
    this->sizeMask = size - 1;
    this->head = 0;
    this->storage.resize(0);
    this->storage.resize(size);
}

#define TEST_ASSERT(x)                                           \
    {                                                            \
        if (!(x))                                                \
        {                                                        \
            throw std::logic_error(SS("Assert failed: " << #x)); \
        }                                                        \
    }

Implementation::DirectConvolutionSection::DirectConvolutionSection(
    size_t size,
    size_t sampleOffset, const std::vector<float> &impulseData, const std::vector<float> *impulseDataRightOpt,
    size_t sectionDelay,
    size_t inputDelay,
    size_t threadNumber)
    : fftPlan(size * 2),
      size(size),
      threadNumber(threadNumber),
      sampleOffset(sampleOffset),
      sectionDelay(sectionDelay),
      inputDelay(inputDelay),
      isStereo(impulseDataRightOpt != nullptr)
{
    buffer.resize(size * 2);
    inputBuffer.resize(size * 2);
    impulseFft.resize(size * 2);
    size_t len = size;

    const float norm = (float)(std::sqrt(2 * size));

    if (sampleOffset >= impulseData.size())
    {
        len = 0;
    }
    else if (sampleOffset + len > impulseData.size())
    {
        len = impulseData.size() - sampleOffset;
    }

    for (size_t i = 0; i < len; ++i)
    {
        impulseFft[i + size] = norm * impulseData[i + sampleOffset];
    }
    fftPlan.Compute(impulseFft, impulseFft, Fft::Direction::Forward);
    bufferIndex = 0;
    if (impulseDataRightOpt != nullptr)
    {
        bufferRight.resize(size * 2);
        inputBufferRight.resize(size * 2);
        impulseFftRight.resize(size * 2);
        for (size_t i = 0; i < len; ++i)
        {
            impulseFftRight[i + size] = norm * (*impulseDataRightOpt)[i + sampleOffset];
        }
        fftPlan.Compute(impulseFftRight, impulseFftRight, Fft::Direction::Forward);
    }
}

void Implementation::DirectConvolutionSection::UpdateBuffer()
{
    fftPlan.Compute(inputBuffer, buffer, Fft::Direction::Forward);
    size_t size2 = size * 2;
    for (size_t i = 0; i < size2; ++i)
    {
        buffer[i] *= impulseFft[i];
    }
    fftPlan.Compute(buffer, buffer, Fft::Direction::Backward);

    if (isStereo)
    {
        fftPlan.Compute(inputBufferRight, bufferRight, Fft::Direction::Forward);
        size_t size2 = size * 2;
        for (size_t i = 0; i < size2; ++i)
        {
            bufferRight[i] *= impulseFftRight[i];
        }
        fftPlan.Compute(bufferRight, bufferRight, Fft::Direction::Backward);
    }
    bufferIndex = 0;
}

BalancedConvolution::~BalancedConvolution()
{
    Close();
}

void BalancedConvolution::Close()
{
    assemblyQueue.Close();
    if (assemblyThread)
    {
        assemblyThread->join();
        assemblyThread = nullptr; // joins the assembly thread.
    }

    // shut down Direct Convolution Threads in an orderly manner.
    for (auto &thread : directSectionThreads)
    {
        thread->Close(); // close the thread's delay line.
    }
    audioThreadToBackgroundQueue.Close(); // shut down all delayLine threads.
}

bool BalancedConvolution::ThreadedDirectSection::Execute(AudioThreadToBackgroundQueue &delayLine)
{
    size_t size = section->directSection.Size();
    bool processed = false;
    while (delayLine.IsReadReady(currentSample, size))
    {
        if (outputDelayLine.CanWrite(size))
        {
            section->directSection.Execute(delayLine, currentSample, outputDelayLine);

            currentSample += size;
            processed = true;
        }
        else
        {
            break;
        }
    }
    return processed;
}

void DirectConvolutionSection::Execute(AudioThreadToBackgroundQueue &input, size_t time, LocklessQueue &output)
{

#if EXECUTION_TRACE
    SectionExecutionTrace::time_point start = SectionExecutionTrace::clock::now();
    auto writeCount = output.GetWriteCount();

#endif

    {
        if (isStereo)
        {
            size_t size = Size();
            for (size_t i = 0; i < size; ++i)
            {
                inputBuffer[i] = inputBuffer[i + size];
            }
            for (size_t i = 0; i < size; ++i)
            {
                inputBufferRight[i] = inputBufferRight[i + size];
            }
            input.ReadRange(time, size, size, inputBuffer, inputBufferRight);
            UpdateBuffer();

            output.Write(size, 0, this->buffer, this->bufferRight);
        }
        else
        {
            size_t size = Size();
            for (size_t i = 0; i < size; ++i)
            {
                inputBuffer[i] = inputBuffer[i + size];
            }
            input.ReadRange(time, size, size, inputBuffer);
            UpdateBuffer();

            output.Write(size, 0, this->buffer);
        }
    }

#if EXECUTION_TRACE
    SectionExecutionTrace::time_point end = SectionExecutionTrace::clock::now();

    if (this->pTrace)
    {
        this->pTrace->Trace(threadNumber, this->Size(), start, end, writeCount, this->inputDelay);
    }

#endif
}

BalancedConvolution::ThreadedDirectSection::ThreadedDirectSection(DirectSection &section)
    : section(&section)
{
    auto &directSection = section.directSection;
    size_t size = directSection.Size();
    (void)size;
    size_t sampleOffset = directSection.SampleOffset();
    size_t sectionDelay = directSection.SectionDelay();
    size_t inputDelay = directSection.InputDelay();

    this->currentSample = inputDelay - size;

    size_t delayLineSize = sampleOffset + sectionDelay + 256;
    outputDelayLine.SetSize(delayLineSize, delayLineSize - size);

    std::vector<float> tempBuffer;
    assert(inputDelay <= size);
    tempBuffer.resize(sampleOffset - (size - inputDelay));
    outputDelayLine.Write(tempBuffer.size(), 0, tempBuffer);
}

void BalancedConvolution::OnSynchronizedSingleReaderDelayLineUnderrun()
{
    ++underrunCount;
}
void BalancedConvolution::OnSynchronizedSingleReaderDelayLineReady()
{
    // hack to allow us to wait on a signle condition variable.
    // If an output delay line writeStalled and now becomes ready, pump the main delay line once
    // to get Execute() to happen once more.
    this->audioThreadToBackgroundQueue.NotifyReadReady();
}

void BalancedConvolution::DirectSectionThread::Execute(AudioThreadToBackgroundQueue &inputDelayLine)
{

    size_t tailPosition = inputDelayLine.GetReadTailPosition();
    while (true)
    {
        bool processed = false;
        for (auto section : sections)
        {
            if (section->Execute(inputDelayLine))
            {
                processed = true;
            }
        }
        if (!processed)
        {
            tailPosition = inputDelayLine.WaitForMoreReadData(tailPosition);
        }
    }
}

void BalancedConvolution::AssemblyThreadProc()
{
    std::vector<float> buffer;
    std::vector<float> bufferRight;
    buffer.resize(16);
    bufferRight.resize(16);

    toob::SetThreadName("cr_assembly");
    try
    {
        // 76 slots into pipewire priorities nicely.
        toob::SetRtThreadPriority(76);
    }
    catch (const std::exception &e)
    {
        SetAssemblyThreadStartupFailed(e.what());
        return;
    }
    SetAssemblyThreadStartupSucceeded();

    try
    {
        if (this->isStereo)
        {
            while (true)
            {
                for (size_t i = 0; i < buffer.size(); ++i)
                {
                    float resultL = 0;
                    float resultR = 0;
                    for (auto &sectionThread : directSectionThreads)
                    {
                        float left,right;
                        sectionThread->Tick(&left,&right);
                        resultL += left;
                        resultR += right;
                    }
                    buffer[i] = resultL;
                    bufferRight[i] = resultR;
                }
                assemblyQueue.Write(buffer, bufferRight, buffer.size());
            }
        }
        else
        {
            while (true)
            {
                for (size_t i = 0; i < buffer.size(); ++i)
                {
                    float result = 0;
                    for (auto &sectionThread : directSectionThreads)
                    {
                        result += sectionThread->Tick();
                    }
                    buffer[i] = result;
                }
                assemblyQueue.Write(buffer, buffer.size());
            }
        }
    }
    catch (const DelayLineClosedException &)
    {
        // expected.
    }
    catch (const std::exception &e)
    {
        throw;
    }
}

void BalancedConvolution::WaitForAssemblyThreadStartup()
{
    std::unique_lock lock{startup_mutex};
    while (true)
    {
        if (startupSucceeded)
        {
            return;
        }
        if (startupError.length() != 0)
        {
            throw std::logic_error(startupError);
        }
        startup_cv.wait(lock);
    }
}
void BalancedConvolution::SetAssemblyThreadStartupFailed(const std::string &e)
{
    {
        std::lock_guard lock{startup_mutex};
        this->startupError = e;
    }
    startup_cv.notify_all();
}
void BalancedConvolution::SetAssemblyThreadStartupSucceeded()
{
    {
        std::lock_guard lock{startup_mutex};
        this->startupSucceeded = true;
    }
    startup_cv.notify_all();
}
