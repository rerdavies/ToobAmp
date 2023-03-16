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

#include "BalancedConvolution.hpp"
#include "../ss.hpp"
#include <memory>
#include <cassert>
#include <limits>
#include <iostream>
#include <unordered_set>
#include <stdexcept>
#include <numbers>
#include <cstdint>
#include <cstddef>
#include "Fft.hpp"
#include <unordered_map>
#include <fstream>
#include <set>
#include "BinaryWriter.hpp"
#include "BinaryReader.hpp"

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

static std::size_t Log2(std::size_t value)
{
    int log = 0;
    while (value > 0)
    {
        ++log;
        value >>= 1;
    }
    return log;
}
static std::size_t Pow2(std::size_t value)
{
    return 1 << value;
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
    double nanosecondsPerSample;
    int threadNumber;
};

constexpr int INVALID_THREAD_ID = -1; // DirectionSections with this size are not encounered in normal use.

std::mutex BalancedConvolution::globalMutex;

static std::vector<ExecutionEntry> executionTimePerSampleNs{
    // n	direct
    {4, 82.402, INVALID_THREAD_ID},
    {8, 75.522, INVALID_THREAD_ID},
    {16, 78.877, INVALID_THREAD_ID},
    {32, 86.127, INVALID_THREAD_ID},
    {64, 92.286, INVALID_THREAD_ID},

    // executed on thread 1.
    {128, 100.439, 1},
    {256, 107.703, 1},
    {512, 155.486, 1},
    {1024, 164.186, 2},
    // executed on thread 2
    {2048, 192.041, 2},
    {4096, 206.026, 2},
    {8192, 241.912, 3},
    {16384, 285.395, 3},
    {32768, 448.843, 4},
    {65536, 575.380, 4},
    {131072, 668.226, 5},
};
constexpr int MAX_THREAD_ID = 6;
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

static void UpdateDirectExecutionLeadTimes(size_t sampleRate,size_t maxAudioBufferSize)
{
    // calculate the lead time in samples based on how long it takes to execute
    // a direction section of a particular size.

    // Calculate per-thread worst execution times.
    // (Service threads handle groups of block sizes.)
    std::vector<int> basicExecutionTime;
    basicExecutionTime.resize(MAX_THREAD_ID + 1);
    for (const auto &entry : executionTimePerSampleNs)
    {
        if (entry.threadNumber != INVALID_THREAD_ID)
        {
            double executionTimeSeconds = entry.n * entry.nanosecondsPerSample * 1E-9;
            executionTimeSeconds *= ((double)sampleRate) / 44100; // benchmarks were for 44100.
            executionTimeSeconds *= 1.8 / 1.5;                    // in case we're running on 1.5Ghz pi.
            executionTimeSeconds *= 2;                            // competing for cache space.
            executionTimeSeconds *= 1.5;                          // because there may be duplicates
            size_t samplesLeadTime = (std::ceil(executionTimeSeconds * sampleRate));

            basicExecutionTime[entry.threadNumber] += samplesLeadTime;
        }
    }

    directSectionLeadTimes.resize(executionTimePerSampleNs.size() + 3);
    for (size_t i = 0; i < directSectionLeadTimes.size(); ++i)
    {
        directSectionLeadTimes[i] = INVALID_EXECUTION_TIME;
    }
    double schedulingJitterSeconds = 0.002;                                         // 2ms for scheduiling overhead.

    size_t schedulingJitter = (size_t)(schedulingJitterSeconds * sampleRate + maxAudioBufferSize);

    for (const auto &entry : executionTimePerSampleNs)
    {
        size_t log2N = Log2(entry.n);
        if (entry.threadNumber != INVALID_THREAD_ID)
        {
            directSectionLeadTimes[log2N] = basicExecutionTime[entry.threadNumber] + schedulingJitter + entry.n;
        }
    }
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

const char *Implementation::FftPlan::MAGIC_FILE_STRING = "FftPlan";

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
#ifndef JUNK
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
#else
namespace LsNumerics::Implementation
{
    class SlotUsage
    {
    private:
        fft_index_t planSize;

        std::vector<uint64_t> bitMask;

        static constexpr size_t BITCOUNT = sizeof(uint64_t) * 8;
        static constexpr size_t ALL_ONES = (size_t)-1;

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
            bitMask.resize((planSize + BITCOUNT - 1) / BITCOUNT);
            this->planSize = ToIndex(planSize);
        }

        fft_index_t borrowedSlot = -1;
        // range: [from,to)
        void Add(fft_index_t from, fft_index_t to)
        {
            if (from == to)
            {
                borrowedSlot = from;
                ++to;
            }
            else
            {
                borrowedSlot = -1;
            }
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
            size_t firstSlot = ((size_t)from) / BITCOUNT;
            size_t lastSlot = ((size_t)to - 1) / BITCOUNT;
            size_t firstMask = ALL_ONES << ((size_t)from % BITCOUNT);
            size_t lastMask = ALL_ONES >> (BITCOUNT - ((size_t)to) % BITCOUNT);

            if (firstSlot == lastSlot)
            {
                bitMask[firstSlot] |= (firstMask & lastMask);
            }
            else
            {
                bitMask[firstSlot] |= (firstMask);
                for (size_t i = firstSlot + 1; i < lastSlot; ++i)
                {
                    bitMask[i] = ALL_ONES;
                }
                if (lastMask != 0)
                {
                    bitMask[lastSlot] |= (lastMask);
                }
            }
        }
        bool contains(fft_index_t time) const
        {
            return contains_any(time, time + 1);
        }
        bool contains_any(fft_index_t from, fft_index_t to) const
        {
            if (from >= planSize)
            {
                if (from == to)
                {
                    to %= planSize;
                }
                from %= planSize;
            }
            if (to > planSize)
            {
                to = to % planSize;
            }
            if (from == to)
            {
                if (borrowedSlot == from)
                {
                    return false;
                }
                ++to;
            }

            size_t firstSlot = ((size_t)from) / BITCOUNT;
            size_t lastSlot = ((size_t)to - 1) / BITCOUNT;
            size_t firstMask = ALL_ONES << ((size_t)from % BITCOUNT);
            size_t lastMask = ALL_ONES >> (BITCOUNT - ((size_t)to) % BITCOUNT);

            if (firstSlot == lastSlot)
            {
                return (bitMask[firstSlot] & firstMask & lastMask) != 0;
            }
            else
            {
                if ((bitMask[firstSlot] & firstMask) != 0)
                {
                    return true;
                }
                if ((lastSlot != bitMask.size() && bitMask[lastSlot] & lastMask) != 0)
                {
                    return true;
                }
                for (size_t i = firstSlot + 1; i < lastSlot; ++i)
                {
                    if (bitMask[i] != 0)
                    {
                        return false;
                    }
                }
            }

            return false;
        }
        void Print(std::ostream &o) const
        {
            o << '[';
            fft_index_t count = ToIndex(bitMask.size() * sizeof(std::uint64_t));

            for (fft_index_t i = 0; i < count; /**/)
            {
                if (contains(i))
                {
                    auto start = i;
                    ++i;
                    while (i < count && contains(i))
                    {
                        ++i;
                    }
                    auto end = i;
                    o << '[' << start << ',' << end << ')';
                }
                else
                {
                    ++i;
                }
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

#endif
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
            if (set.contains(op))
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

static std::size_t ReverseBits(std::size_t value, std::size_t nBits)
{
    std::size_t result = 0;
    for (std::size_t i = 0; i < nBits; ++i)
    {
        result = (result << 1) | (value & 1);
        value >>= 1;
    }
    return result;
}
static std::vector<int> MakeReversedBits(std::size_t size)
{
    std::vector<int> result;
    result.resize(size);
    std::size_t log2 = Log2(size) - 1;
    for (std::size_t i = 0; i < size; ++i)
    {
        result[i] = ReverseBits(i, log2);
    }
    return result;
}

static inline fft_complex_t M(std::size_t k, std::size_t n, FftDirection direction)
{
    static constexpr double TWO_PI = (fft_float_t)(std::numbers::pi * 2);
    // e^(2 PI k)/n
    std::complex<double> t = std::exp(std::complex<double>(0, ((int)(direction)) * (TWO_PI * k / n)));
    return fft_complex_t((fft_float_t)(t.real()), (fft_float_t)(t.imag()));
}
namespace LsNumerics::Implementation
{
    class Builder
    {
    public:
        void MakeFft(std::size_t size, FftDirection direction)
        {
            this->planSize = size;
            std::vector<FftOp::op_ptr> orderedInputs = MakeInputs(size);
            this->inputs = orderedInputs;
            this->outputs = MakeFft(orderedInputs, direction);

            this->maxOpsPerCycle = (Log2(inputs.size())) / 2;    // the absolute minimum.
            this->maxOpsPerCycle = this->maxOpsPerCycle * 4 / 3; // provide some slack.
            this->startingSlot = 0;
        }

        FftOp::op_ptr MakeConvolutionConstant(const fft_complex_t &value)
        {
            FftOp::op_ptr result = std::make_shared<ConstantOp>(value);
            return result;
        }

        FftOp::op_ptr MakeConstant(const fft_complex_t &value)
        {
            if (constantCache.contains(value))
            {
                return constantCache[value];
            };
            FftOp::op_ptr result = std::make_shared<ConstantOp>(value);
            constants.push_back(result);
            constantCache[value] = result;
            return result;
        }
        std::vector<FftOp::op_ptr> MakeInputs(size_t size)
        {
            std::vector<FftOp::op_ptr> orderedInputs;
            for (std::size_t i = 0; i < size; ++i)
            {

                FftOp::op_ptr in{new LsNumerics::Implementation::InputOp(i, this->planSize)};

                orderedInputs.emplace_back(std::move(in));
            }
            return orderedInputs;
        }
        std::vector<FftOp::op_ptr> MakeFft(std::vector<FftOp::op_ptr> &orderedInputs, FftDirection direction)
        {
            size_t size = orderedInputs.size();

            std::size_t layers = Log2(size);
            assert(layers >= 2);
            std::vector<int> reversedBits = MakeReversedBits(size);

            // swap inputs into bit-reversed order.
            std::vector<FftOp::op_ptr> inputs;
            inputs.resize(orderedInputs.size());
            for (std::size_t i = 0; i < inputs.size(); ++i)
            {
                inputs[reversedBits[i]] = orderedInputs[i];
            }

            std::vector<FftOp::op_ptr> outputs;
            for (std::size_t stage = 0; stage < layers - 1; ++stage)
            {
                outputs.resize(size);
                std::size_t stride = Pow2(stage);
                std::size_t groupStride = stride * 2;

                for (std::size_t group = 0; group < size; group += groupStride)
                {
                    for (std::size_t i = 0; i < stride; ++i)
                    {
                        int in0 = group + i;
                        int in1 = group + i + stride;
                        fft_complex_t m = ::M(i % stride, groupStride, direction);
                        FftOp::op_ptr t(new LsNumerics::Implementation::ButterflyOp(inputs[in0], inputs[in1], MakeConstant(m)));
                        outputs[in0] = FftOp::op_ptr(new LeftOutputOp(t));
                        outputs[in1] = FftOp::op_ptr(new RightOutputOp(t));
                    }
                }
                inputs = outputs;
                outputs.resize(0);
            }
            return inputs;
        }

        std::vector<FftOp::op_ptr> MakeHalfConvolutionSection(
            std::vector<FftOp::op_ptr> &inputs,
            std::vector<FftOp::op_ptr> &impulseFftConstants)
        {
            std::vector<FftOp::op_ptr> inverseInputs = MakeFft(inputs, FftDirection::Forward);

            auto opZero = MakeConstant(0);
            // use a hacked butterfly op  multiply with Fft of impusle data.
            std::vector<FftOp::op_ptr> convolvedInputs;

            for (size_t i = 0; i < inverseInputs.size(); ++i)
            {
                FftOp::op_ptr convolveOp{
                    new ButterflyOp(opZero, inverseInputs[i], impulseFftConstants[i])};
                convolveOp = std::make_shared<LeftOutputOp>(convolveOp);
                convolvedInputs.push_back(convolveOp);
            }

            auto result = MakeFft(convolvedInputs, FftDirection::Reverse);
            // discard the first half, use the second half.
            return std::vector<FftOp::op_ptr>(result.begin(), result.begin() + +result.size() / 2);
        }

        plan_ptr MakeConvolutionSection(std::size_t size)
        {
            this->planSize = size * 2;
            std::vector<FftOp::op_ptr> orderedInputs = MakeInputs(size * 3);
            this->inputs = orderedInputs;

            for (size_t i = 0; i < size * 2; ++i)
            {
                impulseFftConstants.push_back(MakeConvolutionConstant(0));
            }
            std::vector<FftOp::op_ptr> firstInputs{orderedInputs.begin(), orderedInputs.begin() + 2 * size};
            std::vector<FftOp::op_ptr> firstSection = MakeHalfConvolutionSection(firstInputs, impulseFftConstants);

            std::vector<FftOp::op_ptr> secondInputs = {orderedInputs.begin() + size, orderedInputs.end()};
            std::vector<FftOp::op_ptr> secondSection = MakeHalfConvolutionSection(secondInputs, impulseFftConstants);

            this->maxOpsPerCycle = (Log2(size * 2)) / 2; // fft is 2 x size.
            this->maxOpsPerCycle += 2;                   // for convolve butterflies.
            this->maxOpsPerCycle *= 2;                   // One FFT, one inverse FFT.

            this->maxOpsPerCycle = this->maxOpsPerCycle * 4 / 3; // provide some slack.

            // this->maxOpsPerCycle = 1024;

            std::vector<FftOp::op_ptr> outputs;
            outputs.reserve(firstSection.size() + secondSection.size());
            outputs.insert(outputs.end(), firstSection.begin(), firstSection.end());
            outputs.insert(outputs.end(), secondSection.begin(), secondSection.end());

            // auto zero = MakeConstant(0);
            // for (size_t i = 0; i < secondSection.size(); ++i)
            // {
            //     outputs.push_back(zero);
            // }

            this->outputs = std::move(outputs);
            this->startingSlot = size;

            auto plan = Build();

            plan->CheckForOverwrites();
            return plan;
        }

    public:
        size_t startingSlot;
        plan_ptr Build();

        size_t Size() const { return inputs.size(); }

        static CompiledButterflyOp CompileOp(ButterflyOp *op)
        {
            auto in0 = op->GetInput(0)->GetStorageIndex();
            auto in1 = op->GetInput(1)->GetStorageIndex();
            auto out = op->GetStorageIndex();
            auto M = op->GetM();
            CompiledButterflyOp result = CompiledButterflyOp(in0, in1, out, M->GetStorageIndex());
#if DEBUG_OPS
            result.id = op->Id();
#endif
            return result;
        }

        std::size_t CalculateMaxDelay()
        {
            std::ptrdiff_t maxDelay = 0;

            for (std::size_t i = 0; i < outputs.size(); ++i)
            {
                auto available = outputs[i]->GetEarliestAvailable();
                if (available >= 0)
                {
                    std::ptrdiff_t delay = available - i;
                    if (delay > maxDelay)
                        maxDelay = delay;
                }
            }
            return (std::size_t)maxDelay;
            ;
        }

    private:
        size_t planSize;

        struct ComplexHash
        {
            size_t operator()(const fft_complex_t &value) const
            {
                return std::hash<double>()(value.real()) + std::hash<double>()(value.imag());
            }
        };
        std::unordered_map<fft_complex_t, FftOp::op_ptr, ComplexHash> constantCache;

        using schedule_t = std::vector<std::vector<FftOp *>>;

        std::vector<FftOp::op_ptr> constants;
        size_t impulseFftOffset = (size_t)-1;
        size_t constantsOffset = (size_t)-1;
        std::vector<FftOp::op_ptr> impulseFftConstants;

        void AllocateMemory()
        {
            IndexAllocator allocator(this->planSize);

            // pre-allocate indices for inputs.
            allocator.Allocate(inputs.size(), nullptr);
            // don't recycle memory for outputs.
            // TODO: We *could* recycle output slots to reduce cache use.
            for (auto &output : outputs)
            {
                output->AddInputReference();
            }
            // LeftOutputOp and RightOutputOp instances aren't visible in the schedule.
            // Call AllocateMemory() to copy the storage indices from the referenced
            // butterflyOps.
            // Allocate first to make sure we don't use a recycled slot.
            for (auto &output : outputs)
            {
                output->AllocateMemory(allocator);
            }

            // allocate convolution fft constants.
            this->impulseFftOffset = allocator.Allocate(0, nullptr);
            allocator.Allocate(this->impulseFftConstants.size(), nullptr);

            for (size_t i = 0; i < impulseFftConstants.size(); ++i)
            {
                this->impulseFftConstants[i]->SetStorageIndex(ToIndex(i + this->impulseFftOffset));
            }
            this->impulseFftOffset = impulseFftConstants[0]->GetStorageIndex();

            // allocate constants.
            this->constantsOffset = allocator.Allocate(0, nullptr);
            size_t constantSize = constants.size();
            ;
            if (constantSize & 1)
            {
                ++constantSize; // maintain memory aligment.
            }

            allocator.Allocate(constantSize, nullptr);

            for (size_t i = 0; i < constants.size(); ++i)
            {
                constants[i]->SetStorageIndex(ToIndex(constantsOffset + i));
            }

            for (std::size_t i = 0; i < schedule.size(); ++i)
            {
                for (auto &op : schedule[i])
                {
                    op->FreeInputReferences(allocator);
                    op->AllocateMemory(allocator);
                }
            }
            this->workingMemorySize = allocator.Allocate(0, nullptr);
        }

        static std::size_t countButterflies(std::vector<FftOp *> schedulerSlot)
        {
            std::size_t count = 0;
            for (auto &op : schedulerSlot)
            {
                if (op->GetOpType() == FftOp::OpType::ButterflyOp)
                {
                    ++count;
                }
            }
            return count;
        }

        using dependency_set = std::unordered_set<FftOp *>;

        void getDependencySet(dependency_set &set, FftOp *op)
        {
            if (op->GetOpType() == FftOp::OpType::ButterflyOp)
            {
                set.insert(op);
            }
            for (auto &input : op->GetInputs())
            {
                getDependencySet(set, input.get());
            }
        }
        void GetPendingOps(std::vector<FftOp *> &ops, FftOp *op)
        {
            if (!op->GetReady())
            {
                op->SetReady();
                for (auto input : op->GetInputs())
                {

                    GetPendingOps(ops, input.get());
                }
                if (op->GetOpType() == FftOp::OpType::ButterflyOp)
                {
                    ops.push_back(op);
                }
            }
        }
        static struct
        {
            bool operator()(FftOp *a, FftOp *b) const
            {
                return (a->GetEarliestAvailable() < b->GetEarliestAvailable());
            }
        } ScheduleOpsComparator;

        schedule_t schedule;
        std::size_t maxOpsPerCycle = 2;
        std::size_t workingMemorySize = (std::size_t)-1;

        std::size_t GetOpCount(std::size_t slot)
        {
            size_t result = 0;

            for (size_t i = slot % this->planSize; i < schedule.size(); i += planSize)
            {
                result += schedule[i].size();
            }
            return result;
        }
        std::size_t ScheduleOp(std::size_t slot, FftOp *op)
        {
            size_t slotsTried = 0;
            while (true)
            {
                std::size_t currentOps = GetOpCount(slot);
                if (currentOps < maxOpsPerCycle)
                {
                    if (slot >= schedule.size())
                    {
                        schedule.resize(schedule.size() + this->planSize);
                    }
                    schedule[slot].push_back(op);
                    op->SetEarliestAvailable(slot);
                    return slot;
                }
                ++slot;
                ++slotsTried;
                if (slotsTried == planSize)
                {
                    throw std::logic_error("Fft scheduling failed.");
                }
            }
        }
        void ScheduleOps()
        {
            // this->maxOpsPerCycle = this->maxOpsPerCycle*this->maxOpsPerCycle+this->maxOpsPerCycle;
            assert(this->planSize == this->outputs.size());
            schedule.resize(0);
            size_t numberOfOps = 0;
            schedule.resize(this->planSize);
            for (std::size_t nOutput = 0; nOutput < this->outputs.size(); ++nOutput)
            {
                std::vector<FftOp *> ops;
                GetPendingOps(ops, this->outputs[nOutput].get());
                for (auto op : ops)
                {
                    op->UpdateEarliestAvailable(); // update earliest here to get O(N LogN)
                }
                std::stable_sort(ops.begin(), ops.end(), ScheduleOpsComparator);
                fft_index_t slot = 0;
                for (auto op : ops)
                {
                    if (op->GetEarliestAvailable() > slot)
                    {
                        slot = op->GetEarliestAvailable();
                    }
                    ++numberOfOps;
                    slot = ScheduleOp(slot, op);
                    op->SetEarliestAvailable(slot);
                }
            }
            (void)numberOfOps;
        }
        void PrintDependencyMap(schedule_t &schedule)
        {
            dependency_set previousSet;

            std::cout << "Dependencies" << std::endl;
            for (std::size_t i = 0; i < this->outputs.size(); ++i)
            {
                dependency_set set;
                getDependencySet(set, this->outputs[i].get());
                for (auto &op : previousSet)
                {
                    set.erase(op);
                }
                std::cout << i << ":" << set.size() << std::endl;
                for (auto &op : set)
                {
                    previousSet.insert(op);
                }
            }
            std::cout << std::endl;
        }
        void PrintDelays()
        {
            std::cout << "Delays" << std::endl;
            std::size_t maxDelay = 0;
            for (std::size_t i = 0; i < outputs.size(); ++i)
            {
                std::size_t delay = outputs[i]->GetEarliestAvailable() - i;
                std::cout << i << ": " << delay << "  ";
                if (delay > maxDelay)
                {
                    maxDelay = delay;
                }
                if ((i + 1) % 8 == 0)
                {
                    std::cout << std::endl;
                }
            }
            std::cout << "max delay: " << maxDelay << std::endl;
        }
        static void PrintOpCounts(schedule_t &schedule)
        {
            std::size_t total = 0;
            for (std::size_t i = 0; i < schedule.size() / 2; ++i)
            {
                std::size_t lopri = countButterflies(schedule[i]);
                std::size_t hipri = countButterflies(schedule[i + schedule.size() / 2]);

                std::cout << i << ": " << lopri << " + " << hipri << " = " << (lopri + hipri) << "  ";
                total += (lopri + hipri);
                if ((i + 1) % 6 == 0)
                {
                    std::cout << std::endl;
                }
            }
            std::cout << "ops=" << total << std::endl;
        }

        std::vector<FftOp::op_ptr> inputs;
        std::vector<FftOp::op_ptr> outputs;
    };

    plan_ptr Builder::Build()
    {
        this->planSize = outputs.size();
        size_t opCount = FftOp::GetTotalOps(outputs);
        this->maxOpsPerCycle = (opCount + planSize - 1) / planSize;
        this->maxOpsPerCycle = maxOpsPerCycle * 3 / 2; // some slack.

        ScheduleOps();
        // PrintOpCounts(schedule);
        // PrintDelays();
        AllocateMemory();

        std::size_t maxDelay = CalculateMaxDelay();
        std::size_t planSize = this->planSize;
        std::size_t workingMemorySize = this->workingMemorySize;
        std::vector<PlanStep> ops;
        fft_index_t discardSlot = workingMemorySize;
        ++workingMemorySize;

        std::size_t numberOfOps = 0;
        for (std::size_t i = 0; i < planSize; ++i)
        {
            PlanStep planStep;
            planStep.inputIndex = (fft_index_t)i;
            if (inputs.size() > planSize)
            {
                assert(inputs.size() <= planSize * 2); // this can be fixed, but not currently implemented.
                if (planStep.inputIndex + planSize < inputs.size())
                {
                    planStep.inputIndex2 = planStep.inputIndex + planSize;
                }
                else
                {
                    planStep.inputIndex2 = discardSlot;
                }
            }
            else
            {
                planStep.inputIndex2 = -1;
            }
            size_t outputIndex = (planSize + i - maxDelay) % planSize;
            planStep.outputIndex = outputs[outputIndex]->GetStorageIndex();

            // schedule %size op latest first.
            for (ptrdiff_t k = schedule.size() - planSize + i; k >= 0; k -= planSize)
            // for (std::size_t k  = i; k < schedule.size(); k += planSize)
            {
                for (auto op : schedule[k])
                {
                    if (op->GetOpType() == FftOp::OpType::ButterflyOp)
                    {
                        ++numberOfOps;
                        planStep.ops.push_back(CompileOp((ButterflyOp *)(op)));
                    }
                }
            }
            ops.push_back(std::move(planStep));
        }
        (void)numberOfOps; // suppress unused warning.

        std::vector<fft_complex_t> compiledConstants;
        compiledConstants.reserve(constants.size());
        for (auto &constant : constants)
        {
            ConstantOp *op = (ConstantOp *)(constant.get());
            compiledConstants.push_back(op->GetValue());
        }
        return std::make_shared<FftPlan>(maxDelay, workingMemorySize, std::move(ops), this->constantsOffset, std::move(compiledConstants), this->startingSlot, this->impulseFftOffset);
    }

}

std::unordered_map<BalancedFft::PlanKey, BalancedFft::plan_ptr, BalancedFft::PlanKeyHash> BalancedFft::planCache;

BalancedFft::plan_ptr BalancedFft::GetPlan(std::size_t size, FftDirection direction)
{
    PlanKey planKey{size, direction};
    plan_ptr plan;
    if (planCache.contains(planKey))
    {
        plan = planCache[planKey];
    }
    else
    {
        Builder builder;
        builder.MakeFft(size, direction);
        plan = builder.Build();
        planCache[planKey] = plan;
    }
    return plan;
}
std::unordered_map<std::size_t, BalancedFft::plan_ptr> BalancedConvolutionSection::planCache;

BalancedFft::BalancedFft(std::size_t size, FftDirection direction)
{
    this->SetPlan(GetPlan(size, direction));
}

void Implementation::FftPlan::PrintPlan()
{
    PrintPlan(std::cout, true);
}
void Implementation::FftPlan::PrintPlan(const std::string &fileName)
{
    std::ofstream o{fileName};
    if (!o)
    {
        throw std::logic_error("Can't open file for output.");
    }
    PrintPlan(o, false);
}
void Implementation::FftPlan::PrintPlan(std::ostream &output, bool trimIds)
{
    using namespace std;
    output << "  Size: " << this->Size() << endl;
    output << "  Delay: " << this->Delay() << endl;
    output << "  ops: [" << endl;
    for (size_t i = 0; i < this->steps.size(); ++i)
    {
        PlanStep &step = steps[i];
        output << "    " << i << ": [" << endl;
        output << "      input: " << step.inputIndex << endl;
        output << "      input2: " << step.inputIndex2 << endl;
        output << "      output: " << step.outputIndex << endl;
        output << "      ops: [" << endl;
        for (auto &op : step.ops)
        {
            output << "        "
                   << op.in0
                   << "," << op.in1
                   << "->" << op.out;
#if DEBUG_OPS
            output << "  " << op.id.substr(0, std::min(op.id.length(), size_t(150)));
            // if (trimIds)
            // {
            // } else {
            //     output << "  " << op.id;
            // }
#endif
            output << endl;
        }
        output << "      ]" << endl;
    }
    output << "    " << endl;
    output << "  ]" << endl;
}

void BalancedFft::SetPlan(plan_ptr plan)
{
    this->plan = plan;
    Reset();
}
void BalancedFft::Reset()
{
    this->workingMemory.resize(0);
    this->workingMemory.resize(plan->StorageSize());
    plan->InitializeConstants(this->workingMemory);
}

void BalancedConvolutionSection::SetPlan(
    plan_ptr plan, size_t offset, const std::vector<float> &impulseData)
{
    this->plan = plan;
    this->size = plan->Size() / 2;

    std::vector<fft_complex_t> fftConvolutionData;
    fftConvolutionData.resize(size * 2);
    {
        std::vector<fft_complex_t> buffer;
        buffer.resize(size * 2);
        size_t len = size;
        if (offset >= impulseData.size())
        {
            len = 0;
        }
        else if (offset + len > impulseData.size())
        {
            len = impulseData.size() - offset;
        }
        const float norm = 1; // (float)(std::sqrt(2 * size)); ?

        for (size_t i = 0; i < len; ++i)
        {
            buffer[i + size] = impulseData[i + offset] * norm;
        }
        Fft normalFft = Fft(size * 2);
        normalFft.Compute(buffer, fftConvolutionData, StagedFft::Direction::Forward);
    }

    // Copy the convolution data in working memory.
    this->workingMemory.resize(0);
    this->workingMemory.resize(plan->StorageSize());
    size_t impulseFftOffset = plan->ImpulseFftOffset();
    for (size_t i = 0; i < size * 2; ++i)
    {
        this->workingMemory[impulseFftOffset + i] = fftConvolutionData[i];
    }
    Reset();
}
void BalancedConvolutionSection::Reset()
{
    // zero out data while preserving the convoltion data.

    for (size_t i = 0; i < plan->ImpulseFftOffset(); ++i)
    {
        this->workingMemory[i] = 0;
    }
    // skip over the convolution data.
    for (size_t i = plan->ImpulseFftOffset() + size * 2; i < workingMemory.size(); ++i)
    {
        this->workingMemory[i] = 0;
    }
    plan->InitializeConstants(this->workingMemory);

    planIndex = plan->StartingIndex();
}

struct SectionDelayCacheEntry
{
    size_t size;
    size_t delay;
};

static std::vector<SectionDelayCacheEntry> sectionDelayCache;

size_t BalancedConvolutionSection::GetSectionDelay(size_t size)
{
    for (size_t i = 0; i < sectionDelayCache.size(); ++i)
    {
        if (sectionDelayCache[i].size == size)
        {
            return sectionDelayCache[i].delay;
        }
    }
    std::vector<float> data;
    data.push_back(0);
    BalancedConvolutionSection testSection{size, data};
    SectionDelayCacheEntry t{size, testSection.Delay()};
    sectionDelayCache.push_back(t);
    return t.delay;
}

static size_t convolutionSampleRate = (size_t)-1;
static size_t convolutionMaxAudioBufferSize = (size_t)-1;

BalancedConvolution::BalancedConvolution(size_t size, const std::vector<float> &impulseResponse, size_t sampleRate,size_t maxAudioBufferSize)
{
    PrepareSections(size, impulseResponse, sampleRate,maxAudioBufferSize);
    PrepareThreads();
}

BalancedConvolution::DirectSectionThread *BalancedConvolution::GetDirectSectionThreadBySize(size_t size)
{
    int threadNumber = GetDirectSectionThreadId(size);
    if (threadNumber == INVALID_THREAD_ID)
    {
        throw std::logic_error("Invalide thread id.");
    }
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
        auto sectionThread = GetDirectSectionThreadBySize(threadedDirectSection->Size());
        sectionThread->AddSection(threadedDirectSection.get());
        threadedDirectSection->SetWriteReadyCallback(dynamic_cast<IDelayLineCallback*>(this));
    }

    for (size_t i = 0; i < directSectionThreads.size(); ++i)
    {
        DirectSectionThread *thread = directSectionThreads[i].get();
        if (thread)
        {
            delayLine.CreateThread(
                [this, thread]()
                {
                    thread->Execute(this->delayLine);
                },
                -(int)thread->GetThreadNumber());
        }
    }
}
void BalancedConvolution::PrepareSections(size_t size, const std::vector<float> &impulseResponse, size_t sampleRate,size_t maxAudioBufferSize)
{
    constexpr size_t INITIAL_SECTION_SIZE = 128;
    constexpr size_t INITIAL_DIRECT_SECTION_SIZE = 128;
    constexpr size_t MAX_BALANCED_SECTION = 132 * 1024;                                // calculating balanced sections larger than this requires too much memory. Can't go larger than this.
    constexpr size_t DIRECT_SECTION_CUTOFF_LIMIT = std::numeric_limits<size_t>::max(); // the point at which balanced sections become faster than direct sections.

    // nb: global data, but constructor is always protected by the cache mutex.

    {
        std::lock_guard lock{globalMutex};
        if (convolutionSampleRate != sampleRate || convolutionMaxAudioBufferSize != maxAudioBufferSize)
        {
            convolutionSampleRate = sampleRate;
            convolutionMaxAudioBufferSize = maxAudioBufferSize;
            UpdateDirectExecutionLeadTimes(sampleRate,maxAudioBufferSize);
        }
    }
    size_t delaySize = -1;
    if (size < INITIAL_SECTION_SIZE)
    {
        directConvolutionLength = size;
        delaySize = directConvolutionLength;
    }
    else
    {
        // Generate lists of sections based on the following criteria.
        // 1. Initially, we need to use balanced sections on the real-time thread to build up enough delayed samples to switch to DirectSections which are much faster.
        // 2. Once we have enough samples to run direct sections (on a background thread), use direct sections.
        // 3. Due to better cache conditioning, balanced sections run much faster for very large sizes (> 32768), so switch back to balanced sections for very large sections (which run on the real-time thread)

        size_t balancedSectionSize = INITIAL_SECTION_SIZE;
        size_t balancedSectionDelay = BalancedConvolutionSection::GetSectionDelay(balancedSectionSize);
        size_t directSectionSize = INITIAL_DIRECT_SECTION_SIZE;
        directConvolutionLength = balancedSectionDelay;
        if (directConvolutionLength > size)
        {
            directConvolutionLength = size;
        }
        delaySize = directConvolutionLength;

        size_t sampleOffset = directConvolutionLength;

        balancedSections.reserve(16);
        directSections.reserve(16);

        while (sampleOffset < size)
        {

            size_t remaining = size - sampleOffset;

            size_t nextBalancedSectionDelay = std::numeric_limits<size_t>::max();
            if (balancedSectionSize < MAX_BALANCED_SECTION) // don't even consider balanced section sizes larger than this. Don't even ask.
            {
                nextBalancedSectionDelay = BalancedConvolutionSection::GetSectionDelay(balancedSectionSize * 2);
            }
            // Update balanced section size.
            if (sampleOffset >= nextBalancedSectionDelay)
            {
                balancedSectionSize *= 2;
                balancedSectionDelay = nextBalancedSectionDelay;
            }
            while (remaining <= balancedSectionSize / 2 && balancedSectionSize > INITIAL_SECTION_SIZE)
            {
                balancedSectionSize = balancedSectionSize / 2;
                balancedSectionDelay = BalancedConvolutionSection::GetSectionDelay(balancedSectionSize);
            }

            size_t directSectionDelay;

            // Pick a candidate Direct section.

            bool canUseDirectSection = false;
            while (true)
            {
                directSectionDelay = GetDirectSectionLeadTime(directSectionSize);
                if (directSectionDelay == std::numeric_limits<size_t>::max())
                {
                    throw std::logic_error("Failed to schedule direct section.");
                }
                if (directSectionDelay > sampleOffset)
                {
                    canUseDirectSection = false;
                    break;
                }

                canUseDirectSection = true;

                // don't increase direct section size if we can reach the end with the current size.
                if (directSectionSize >= remaining)
                {
                    break;
                }
                // don't increase the direct section size if we don't have enough samples.
                size_t nextDirectSectionDelay = GetDirectSectionLeadTime(directSectionSize * 2);
                if (nextDirectSectionDelay > sampleOffset)
                {
                    break;
                }
                directSectionSize = directSectionSize * 2;
            }

            // if we can use a shorter section size for the last section, do so.
            while (remaining <= balancedSectionSize / 2 && balancedSectionSize > INITIAL_SECTION_SIZE)
            {
                balancedSectionSize = balancedSectionSize / 2;
                balancedSectionDelay = BalancedConvolutionSection::GetSectionDelay(balancedSectionSize);
            }
            while (remaining <= directSectionSize / 2 && directSectionSize > INITIAL_SECTION_SIZE)
            {
                directSectionSize = directSectionSize / 2;
                directSectionDelay = GetDirectSectionLeadTime(directSectionSize);
            }

            bool useBalancedSection = !canUseDirectSection;
            if (directSectionSize >= DIRECT_SECTION_CUTOFF_LIMIT && balancedSectionSize >= DIRECT_SECTION_CUTOFF_LIMIT)
            {
                useBalancedSection = true;
            }
            if (useBalancedSection)
            {
                size_t inputDelay = sampleOffset - balancedSectionDelay;

#if DISPLAY_SECTION_ALLOCATIONS
                std::cout << "balanced "
                          << "sampleOffset: " << sampleOffset
                          << " SectionSize: " << balancedSectionSize
                          << " sectionDelay: " << balancedSectionDelay
                          << " input delay: " << inputDelay
                          << std::endl;
#endif

                if (inputDelay > delaySize)
                {
                    delaySize = inputDelay;
                }
                balancedSections.emplace_back(
                    Section{
                        inputDelay,
                        BalancedConvolutionSection(
                            balancedSectionSize,
                            sampleOffset,
                            impulseResponse)});
                sampleOffset += balancedSectionSize;
            }
            else
            {

                size_t inputDelay = sampleOffset - directSectionDelay;

#if DISPLAY_SECTION_ALLOCATIONS
                std::cout << "direct   "
                          << "sampleOffset: " << sampleOffset
                          << " SectionSize: " << directSectionSize
                          << " sectionDelay: " << directSectionDelay
                          << " input delay: " << inputDelay
                          << std::endl;
#endif

                size_t myDelaySize = sampleOffset + directSectionSize + 256; //long enough to survive an underrun.
                if (myDelaySize > delaySize)
                {
                    delaySize = myDelaySize;
                }

                directSections.emplace_back(
                    DirectSection{
                        inputDelay,
                        DirectConvolutionSection(
                            directSectionSize,
                            sampleOffset,
                            impulseResponse, directSectionDelay)});
                sampleOffset += directSectionSize;
            }
        }
    }
    directImpulse.resize(directConvolutionLength);
    for (size_t i = 0; i < directConvolutionLength; ++i)
    {
        directImpulse[i] = i < impulseResponse.size() ? impulseResponse[i] : 0;
    }
    delayLine.SetSize(delaySize + 1, 256);
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

void BalancedFft::PrintPlan()
{
    this->plan->PrintPlan();
}
#define TEST_ASSERT(x)                                           \
    {                                                            \
        if (!(x))                                                \
        {                                                        \
            throw std::logic_error(SS("Assert failed: " << #x)); \
        }                                                        \
    }

void SlotUsageSearchTest(const std::vector<fft_index_t> &values)
{
    // checks that the binary search for contains_any is working
    {
        // non-borrowable
        SlotUsage slotUsage(256);
        for (size_t value : values)
        {
            slotUsage.Add(value, value + 1);
        }

        for (size_t value : values)
        {
            TEST_ASSERT(!slotUsage.contains(value - 1));
            TEST_ASSERT(slotUsage.contains(value));
            TEST_ASSERT(!slotUsage.contains(value + 1));
        }
    }
    {
        // borrowable
        SlotUsage slotUsage(256);
        for (size_t value : values)
        {
            slotUsage.Add(value, value);
        }

        for (size_t value : values)
        {
            TEST_ASSERT(!slotUsage.contains(value - 1));
            TEST_ASSERT(slotUsage.contains(value));
            TEST_ASSERT(slotUsage.contains_any(value - 1, value + 1));
            TEST_ASSERT(!slotUsage.contains(value + 1));
            TEST_ASSERT(!slotUsage.contains_any(value, value));
            TEST_ASSERT(!slotUsage.contains_any(value - 1, value - 1));
            TEST_ASSERT(!slotUsage.contains_any(value + 1, value + 1));
        }
    }
}

void Implementation::SlotUsageTest()
{
    SlotUsageSearchTest({1, 9, 42, 56, 58, 61, 63, 70, 91});
    {
        SlotUsage slotUsage(256);
        slotUsage.Add(0, 84);
        slotUsage.Add(84, 87);

        TEST_ASSERT(slotUsage.contains(0));
        TEST_ASSERT(slotUsage.contains(86));
        TEST_ASSERT(!slotUsage.contains(87));
        TEST_ASSERT(slotUsage.contains_any(86, 87));
        TEST_ASSERT(!slotUsage.contains_any(87, 256));
        TEST_ASSERT(slotUsage.contains_any(250, 300));

        TEST_ASSERT(slotUsage.contains_any(86, 86));
        TEST_ASSERT(!slotUsage.contains_any(87, 87));
        slotUsage.Add(88, 88);
        TEST_ASSERT(!slotUsage.contains_any(88, 88)); // borrowed slot.
        TEST_ASSERT(slotUsage.contains(88));          // borrowed slot.
    }
    {
        SlotUsage slotUsage(256);
        slotUsage.Add(238, 256 + 10);
        for (fft_index_t i = 0; i < 10; ++i)
        {
            TEST_ASSERT(slotUsage.contains(i));
        }
        for (fft_index_t i = 10; i < 238; ++i)
        {
            TEST_ASSERT(!slotUsage.contains(i));
        }
        for (fft_index_t i = 238; i < 256; ++i)
        {
            TEST_ASSERT(slotUsage.contains(i));
        }
    }
    {
        SlotUsage slotUsage(256);
        slotUsage.Add(255, 256 + 10);
        slotUsage.Add(10, 10);

        slotUsage.Add(10, 12);

        TEST_ASSERT(slotUsage.contains(9));
        TEST_ASSERT(slotUsage.contains(10));
        TEST_ASSERT(!slotUsage.contains(12));
        TEST_ASSERT(slotUsage.contains_any(10, 11));
        TEST_ASSERT(slotUsage.contains_any(10, 10));
        TEST_ASSERT(slotUsage.contains_any(11, 15));

        TEST_ASSERT(slotUsage.contains_any(11, 13));
        TEST_ASSERT(slotUsage.contains_any(11, 11));
        TEST_ASSERT(!slotUsage.contains_any(12, 13));
    }
    {
        SlotUsage slotUsage(256);
        slotUsage.Add(0, 10);
        slotUsage.Add(12, 12);

        TEST_ASSERT(slotUsage.contains(9));
        TEST_ASSERT(!slotUsage.contains(10));
        TEST_ASSERT(slotUsage.contains(12));
        TEST_ASSERT(!slotUsage.contains_any(12, 12));
        TEST_ASSERT(slotUsage.contains_any(9, 9));

        TEST_ASSERT(slotUsage.contains(12));
        TEST_ASSERT(!slotUsage.contains_any(11, 12)); // yes we can re-use the slot.

        TEST_ASSERT(slotUsage.contains_any(12, 13));  // yes we can re-use the slot.
        TEST_ASSERT(!slotUsage.contains_any(13, 14)); // yes we can re-use the slot.
        TEST_ASSERT(slotUsage.contains_any(11, 13));
        TEST_ASSERT(!slotUsage.contains_any(12, 12));

        TEST_ASSERT(!slotUsage.contains_any(13, 13));

        slotUsage.Add(13, 13);
        slotUsage.Add(13, 14);

        slotUsage.Add(17, 17);

        slotUsage.Add(16, 17);
    }
}

void Implementation::FftPlan::CheckForOverwrites()
{
    // simulate execution of the plan, but trace the input generation,
    // ensuring that ops never operate on data from two different generations
    // and that outputs are of the correct generation.

    std::vector<int> workingGenerations;
    workingGenerations.resize(this->storageSize);

    constexpr int UNINITIALIZED_GENERATION = -1;
    constexpr int CONSTANT_GENERATION = -2;

    for (size_t i = 0; i < workingGenerations.size(); ++i)
    {
        workingGenerations[i] = UNINITIALIZED_GENERATION; // uninitialized.
    }
    for (size_t i = 0; i < constants.size(); ++i)
    {
        workingGenerations[i + this->constantsOffset] = CONSTANT_GENERATION;
    }

    int expectedOutputGeneration = -1;
    int outputDelay = Delay();
    size_t stepIndex = 0;

    for (int generation = 0; generation < 20; ++generation)
    {
        for (size_t i = 0; i < steps.size(); ++i)
        {
            auto &step = steps[stepIndex];

            if (step.inputIndex2 != CONSTANT_INDEX)
            {
                workingGenerations[step.inputIndex2] = workingGenerations[step.inputIndex];
            }
            workingGenerations[step.inputIndex] = generation;

            for (auto &op : step.ops)
            {
                auto inGenerationL = workingGenerations[op.in0];
                auto inGenerationR = workingGenerations[op.in1];
                int outGeneration;
                if (inGenerationL < 0)
                {
                    outGeneration = inGenerationR;
                }
                else if (inGenerationR < 0)
                {
                    outGeneration = inGenerationL;
                }
                else
                {
                    assert(inGenerationL == inGenerationR);
                    outGeneration = inGenerationL;
                }
                workingGenerations[op.out] = outGeneration;
                workingGenerations[op.out + 1] = outGeneration;
            }
            int outputGeneration = workingGenerations[step.outputIndex];
            if (outputGeneration != CONSTANT_GENERATION)
            {
                if (outputGeneration != expectedOutputGeneration)
                {
                    std::cout << "Output is wrong generation."
                              << "  generation: " << generation << " step: " << (&step - (&steps[0]))
                              << " expected: " << expectedOutputGeneration
                              << " actual: " << outputGeneration
                              << std::endl;
                }
            }
            if (--outputDelay == 0)
            {
                ++expectedOutputGeneration;
                outputDelay = this->steps.size();
            }

            ++stepIndex;
            if (stepIndex == steps.size())
            {
                stepIndex = 0;
            }
        }
    }
}

Implementation::CompiledButterflyOp::CompiledButterflyOp(BinaryReader &reader)
{


    // undo compression optimizations.
    reader >> in0 >> in1 >> out >> M_index;
    in1 += in0;
}

void Implementation::CompiledButterflyOp::Write(BinaryWriter &writer) const
{
    // adjust values in an attempt to improve compressibility.
    writer << in0 << (in1-in0) << out << M_index;
}

Implementation::PlanStep::PlanStep(BinaryReader &reader)
{
    reader >> inputIndex >> inputIndex2 >> outputIndex;

    size_t opsSize;
    reader >> opsSize;
    ops.reserve(opsSize);
    for (size_t i = 0; i < opsSize; ++i)
    {
        ops.push_back(CompiledButterflyOp(reader));
    }
}

void Implementation::PlanStep::Write(BinaryWriter &writer) const
{
    writer << inputIndex << inputIndex2 << outputIndex;

    writer << ops.size();
    for (const auto &op : ops)
    {
        op.Write(writer);
    }
}

static void ThrowFormatError()
{
    throw std::logic_error("Invalid file format.");
}

FftPlan::FftPlan(BinaryReader &reader)
{
    const char *p = FftPlan::MAGIC_FILE_STRING;
    char c;
    while (*p)
    {
        reader >> c;
        if (c != *p)
        {
            ThrowFormatError();
        }
        ++p;
    }
    reader >> c;
    if (c != 0)
    {
        ThrowFormatError();
    }
    uint64_t version;
    reader >> version;
    if (version != FftPlan::FILE_VERSION)
    {
        throw std::logic_error("Invalid file version.");
    }

    reader >> norm >> maxDelay >> storageSize;

    size_t stepsSize;
    reader >> stepsSize;
    steps.reserve(stepsSize);

    for (size_t i = 0; i < stepsSize; ++i)
    {
        steps.push_back(PlanStep(reader));
    }

    reader >> this->constantsOffset;
    size_t constantsSize;
    reader >> constantsSize;
    constants.reserve(constantsSize);
    for (size_t i = 0; i < constantsSize; ++i)
    {
        fft_complex_t value;
        reader >> value;
        constants.push_back(value);
    }
    reader >> startingIndex >> impulseFftOffset;

    uint64_t magicTail;
    reader >> magicTail;
    if (magicTail != MAGIC_TAIL_CONSTANT)
    {
        throw std::logic_error("File data is corrupted.");
    }
}

void Implementation::FftPlan::Write(BinaryWriter &writer) const
{

    const char *p = FftPlan::MAGIC_FILE_STRING;
    while (*p)
    {
        writer << *p++;
    }
    writer << (char)0;
    writer << FftPlan::FILE_VERSION;

    writer << norm
           << maxDelay
           << storageSize;

    writer << steps.size();
    for (const auto &step : steps)
    {
        step.Write(writer);
    }
    writer << constantsOffset;
    writer << constants.size();
    for (const auto &value : constants)
    {
        writer << value;
    }
    writer << startingIndex
           << impulseFftOffset;
    writer << MAGIC_TAIL_CONSTANT;
}

BalancedConvolutionSection::BalancedConvolutionSection(std::size_t size, size_t offset, const std::vector<float> &data)
{
    this->SetPlan(GetPlan(size), offset, data);
}

BalancedConvolutionSection::BalancedConvolutionSection(
    const std::filesystem::path &path,
    size_t offset, const std::vector<float> &data)
{
    try
    {
        SetPlan(GetPlan(path), offset, data);
    }
    catch (const std::exception &e)
    {
        throw std::logic_error(SS("Can't open convolution plan file. " << e.what()));
    }
}

std::mutex BalancedConvolutionSection::planCacheMutex;

void BalancedConvolutionSection::ClearPlanCache()
{
    std::lock_guard lock(planCacheMutex);

    planCache.clear();
}
BalancedFft::plan_ptr BalancedConvolutionSection::GetPlan(std::size_t size)
{
    std::lock_guard lock(planCacheMutex);
    if (planCache.contains(size))
    {
        return planCache[size];
    }
    plan_ptr plan;
    if (PlanFileExists(size))
    {
        BinaryReader reader(GetPlanFilePath(size));
        plan = std::make_shared<FftPlan>(reader);
    }
    else
    {
        Builder builder;
        plan = builder.MakeConvolutionSection(size);
    }
    planCache[size] = plan;
    return plan;
}

plan_ptr BalancedConvolutionSection::GetPlan(const std::filesystem::path &path)
{
    std::filesystem::path fullyQualifiedPath = std::filesystem::canonical(path);
    BinaryReader reader(fullyQualifiedPath);
    try
    {
        return std::make_shared<FftPlan>(reader);
    }
    catch (const std::exception &e)
    {
        throw std::logic_error(SS(e.what() << " " << fullyQualifiedPath.string()));
    }
}

void BalancedConvolutionSection::Save(const std::filesystem::path &path)
{
    try
    {
        BinaryWriter writer(path);
        plan->Write(writer);
    }
    catch (const std::exception &e)
    {
        throw std::logic_error(SS("Can't create convolution plan file. " << e.what() << " (" << path.string() << ")"));
    }
}

std::filesystem::path BalancedConvolutionSection::planFileDirectory;

std::filesystem::path BalancedConvolutionSection::GetPlanFilePath(size_t size)
{
    if (planFileDirectory.string().length() == 0)
    {
        throw std::logic_error("PlanFileDirectory not set.");
    }
    std::filesystem::path gzDirectory = BalancedConvolutionSection::planFileDirectory.string()+".gz";
    std::filesystem::path gzPath = gzDirectory / SS(size << ".convolutionPlan.gz");
    if (std::filesystem::exists(gzPath))
    {
        return gzPath;
    }

    return BalancedConvolutionSection::planFileDirectory / SS(size << ".convolutionPlan");
}

bool BalancedConvolutionSection::PlanFileExists(size_t size)
{
    if (planFileDirectory.string().length() == 0)
    {
        return false;
    }
    std::filesystem::path path = GetPlanFilePath(size);
    return std::filesystem::exists(path);
}

Implementation::DirectConvolutionSection::DirectConvolutionSection(
    size_t size,
    size_t offset, const std::vector<float> &impulseData,
    size_t schedulerDelay)
    : fftPlan(size * 2),
      size(size)
{
    this->offset = offset;
    if (schedulerDelay == 0)
        schedulerDelay = size;
    this->schedulerDelay = schedulerDelay;
    buffer.resize(size * 2);
    inputBuffer.resize(size * 2);
    impulseFft.resize(size * 2);
    size_t len = size;

    const float norm = (float)(std::sqrt(2 * size));

    if (offset >= impulseData.size())
    {
        len = 0;
    }
    else if (offset + len > impulseData.size())
    {
        len = impulseData.size() - offset;
    }

    for (size_t i = 0; i < len; ++i)
    {
        impulseFft[i + size] = norm * impulseData[i + offset];
    }
    fftPlan.Compute(impulseFft, impulseFft, Fft::Direction::Forward);
    bufferIndex = 0;
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
    bufferIndex = 0;
}

BalancedConvolution::~BalancedConvolution()
{
    Close();
}

void BalancedConvolution::Close()
{
    // shut down Direct Convolution Threads in an orderly manner.
    for (auto &thread : directSectionThreads)
    {
        thread->Close(); // close the thread's delay line.
    }
    delayLine.Close(); // shut down all delayLine threads.
}

bool BalancedConvolution::ThreadedDirectSection::Execute(SynchronizedDelayLine &delayLine)
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
        } else {
            break;
        }
    }
    return processed;
}

void DirectConvolutionSection::Execute(SynchronizedDelayLine &input, size_t time, SynchronizedSingleReaderDelayLine &output)
{
    size_t size = Size();
    for (size_t i = 0; i < size; ++i)
    {
        inputBuffer[i] = inputBuffer[i + size];
    }
    input.ReadRange(time, size, size, inputBuffer);
    UpdateBuffer();

    // if (TRACE_DELAY_LINE_MESSAGES)
    // {
    //     TraceDelayLineMessage(SS("Write buffer[" << size << "] t=" << time ));
    // }
    output.Write(size, 0, this->buffer);
}

BalancedConvolution::ThreadedDirectSection::ThreadedDirectSection(DirectSection &section)
    : section(&section)
{
    auto &directSection = section.directSection;
    size_t size = directSection.Size();
    (void)size;
    size_t sampleOffset = directSection.SampleOffset();
    size_t sectionDelay = directSection.Delay();
    size_t inputDelay = sampleOffset - sectionDelay;
    (void)inputDelay;

    this->currentSample = 0;

    size_t delayLineSize = sampleOffset + sectionDelay+ 256;
    outputDelayLine.SetSize(delayLineSize,delayLineSize-size);
    std::vector<float> tempBuffer;
    tempBuffer.resize(sampleOffset);
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
    this->delayLine.NotifyReadReady();
}


void BalancedConvolution::DirectSectionThread::Execute(SynchronizedDelayLine&inputDelayLine)
{

    size_t tailPosition = inputDelayLine.GetReadTailPosition();
    while (true)
    {
        bool processed = false;
        for (auto section: sections)
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
