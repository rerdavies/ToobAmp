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

#include "BalancedFft.hpp"
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

using namespace LsNumerics;

// only generate node IDs in debug mode (very expensive)
#define DEBUG_OPS 0
#if DEBUG_OPS
#define SS_ID(x) SS(x)
#else
#define SS_ID(x) ""
#endif

// When enabled, reduces workingMemory size as much as possible
// by re-using slots in workingMemory whenever possible.
#define RECYCLE_SLOTS 1
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
            this->planSize = (fft_index_t)(planSize);
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
            if (time > planSize)
            {
                time -= planSize;
            }
            for (auto &entry : used)
            {
                if (time >= entry.from && time < entry.to)
                    return true;
            }
            return false;
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
                if (contains_any(0,to)) return true;
                return contains_any(from,planSize);
            }
            if (from == to) // a temporary borrow may not overwrite existing data.
            {
                for (auto &entry : used)
                {
                    if (from < entry.to && from >= entry.from)
                    {
                        return true;
                    }
                }

            } else {
                for (auto &entry : used)
                {
                    if (from < entry.to && to > entry.from)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
        void Print(std::ostream&o) const
        {
            o << '[';
            for (const auto&entry: used)
            {
                o << '(' << entry.from << ',' << entry.to << ')';
            }
            o << ']';
        }
        void Print() const {
            Print(std::cout);
            std::cout << std::endl;
        }
        std::string ToString() const;

    };

    std::string SlotUsage::ToString() const {
        std::stringstream s;
        Print(s);
        return s.str();
    }
    std::ostream&operator<<(std::ostream&o,const SlotUsage&slotUsage)
    {
        slotUsage.Print(o);
        return o;
    }

    class FftOp;

    class IndexAllocator
    {

    public:
        std::size_t recycledInputs = 0;
        std::size_t discardedInputs = 0;

        IndexAllocator(size_t planSize)
        {
            this->planSize = planSize;
        }

        std::unordered_map<fft_index_t, SlotUsage> slotUsages;

        fft_index_t Allocate(std::size_t entries, FftOp *op);
        void Free(fft_index_t index, std::size_t size, FftOp *op);

    private:
        struct FreeIndexEntry
        {
            fft_index_t index;
            fft_index_t lastUsed;
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
        }
        virtual std::string Id() const = 0;

        virtual void AllocateMemory(IndexAllocator &allocator) = 0;
        virtual void FreeInputReferences(IndexAllocator &allocator) {}
        virtual void FreeStorageReference(IndexAllocator &allocator, FftOp*op) {}
        virtual void AddInputReference() {}

        void AddInput(op_ptr op)
        {
            this->inputs.push_back(op);
            op->outputs.push_back(this);
            fft_index_t inputT = op->GetEarliestAvailable();
            assert(inputT != INVALID_INDEX);
            if (inputT > earliest)
            {
                earliest = inputT;
            }
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
        FftOp*GetOutput(size_t index)
        {
            return outputs[index];
        }
        const FftOp*GetOutput(size_t index) const
        {
            return outputs[index];
        }

        virtual fft_index_t GetLatestUse() const {
            fft_index_t result = GetEarliestAvailable();
            for (auto output: outputs)
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
        InputOp(std::size_t t)
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
            SetStorageIndex((fft_index_t)t);
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
        virtual fft_index_t GetLatestUse() const {
            const auto&outputs = this->GetOutputs();
            if (outputs.size() == 0) return GetEarliestAvailable();
            return GetOutput(0)->GetEarliestAvailable();
        }
        virtual fft_index_t GetEarliestAvailable() const
        {
            return GetInput(0)->GetEarliestAvailable();
        }

        virtual void FreeStorageReference(IndexAllocator &allocator, FftOp*op)
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
        virtual fft_index_t GetLatestUse() const {
            const auto&outputs = GetOutputs();
            if (outputs.size() == 0)  // eg an output node.
            {
                return this->GetEarliestAvailable();
            }   
            return GetOutput(0)->GetEarliestAvailable();
        }

        virtual void FreeStorageReference(IndexAllocator &allocator, FftOp*op)
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
        virtual void FreeStorageReference(IndexAllocator &allocator,FftOp*op)
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
        if (size == 2 && op != nullptr)
        {
            fft_index_t currentTime = op->GetEarliestAvailable();
            fft_index_t expiryTime = op->GetLatestUse();
            auto &usage = slotUsages[index];

            // std::cout << "Free: " << index << "  from: " << currentTime << " to: " << expiryTime << " " << usage << std::endl;
            usage.SetPlanSize(this->planSize/2);
            usage.Add(currentTime,expiryTime);

            for (auto &i: freeIndices)
            {
                if (i.index == index)
                {
                    throw std::logic_error("Double free.");
                }
            }
            freeIndices.push_back(FreeIndexEntry{index, op->GetEarliestAvailable()});
        }
    }
    fft_index_t IndexAllocator::Allocate(std::size_t entries, FftOp *op)
    {

#if RECYCLE_SLOTS

        if (entries == 2 && op != nullptr && freeIndices.size() != 0)
        {
            fft_index_t currentTime = op->GetEarliestAvailable();
            fft_index_t expiryTime = op->GetLatestUse();

            for (auto i = freeIndices.begin(); i != freeIndices.end(); ++i)
            {
                auto &entry = (*i);
                auto &usage = slotUsages[entry.index];
                usage.SetPlanSize(this->planSize/2);
                if (!usage.contains_any(currentTime,expiryTime))
                {
                    auto result = entry.index;
                    // std::cout << "Allocate: time: " << currentTime << " index: " << entry.index 
                    //     << " [" << currentTime << "," << expiryTime << ")" << " " << usage << std::endl;
                    usage.contains_any(currentTime,expiryTime);
                    freeIndices.erase(i);
                    recycledInputs++;
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
    int pow2 = 1;
    while (value > 0)
    {
        pow2 *= 2;
        --value;
    }
    return pow2;
}

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
            std::vector<FftOp::op_ptr> orderedInputs = MakeInputs(size);
            this->inputs = orderedInputs;
            this->outputs = MakeFft(orderedInputs, direction);

            this->maxOpsPerCycle = (Log2(inputs.size())) / 2;    // the absolute minimum.
            this->maxOpsPerCycle = this->maxOpsPerCycle * 4 / 3; // provide some slack.
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

                FftOp::op_ptr in{new LsNumerics::Implementation::InputOp(i)};

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
        plan_ptr MakeConvolutionSection(std::size_t size, size_t offset, std::vector<float> &data)
        {
            std::vector<FftOp::op_ptr> orderedInputs = MakeInputs(size * 2);
            this->inputs = orderedInputs;

            std::vector<fft_complex_t> fftData;
            fftData.resize(size * 2);
            {
                if (offset >= data.size())
                {
                    throw std::logic_error("No impulse data.");
                }
                std::vector<fft_complex_t> buffer;
                buffer.resize(size * 2);
                size_t len = size;
                if (len > data.size() - offset)
                {
                    len = data.size() - offset;
                }
                for (size_t i = 0; i < len; ++i)
                {
                    buffer[i + size] = data[i + offset];
                }
                Fft<fft_float_t> normalFft(size * 2);
                normalFft.forward(buffer, fftData);
            }

            std::vector<FftOp::op_ptr> inverseInputs = MakeFft(orderedInputs, FftDirection::Forward);

            auto opZero = MakeConstant(0);
            // use a hacked butterfly op  multiply with Fft of impusle data.
            std::vector<FftOp::op_ptr> convolvedInputs;

            for (size_t i = 0; i < inverseInputs.size(); ++i)
            {
                auto m = fftData[i];
                FftOp::op_ptr convolveOp{
                    new ButterflyOp(opZero, inverseInputs[i], MakeConstant(m))};
                convolveOp = std::make_shared<LeftOutputOp>(convolveOp);
                convolvedInputs.push_back(convolveOp);
            }

            this->outputs = MakeFft(convolvedInputs, FftDirection::Reverse);

            this->maxOpsPerCycle = (Log2(inputs.size())) / 2;    // the absolute minimum.
            this->maxOpsPerCycle *= 2;                           // because we're doing 2 FFts.
            this->maxOpsPerCycle += 1;                           // for convolve butterflies.
            this->maxOpsPerCycle = this->maxOpsPerCycle * 4 / 3; // provide some slack.

            auto plan = Build();
            // make the plan return zero for the first half of the result.
            for (size_t i = 0; i < size; ++i)
            {
                plan->ZeroOutput(i + size, opZero->GetStorageIndex());
            }
            return plan;
        }

    public:
        plan_ptr Build()
        {
            ScheduleOps();
            // PrintOpCounts(_schedule);
            // PrintDelays();
            AllocateMemory();
            CheckForOverwrites();

            std::size_t maxDelay = CalculateMaxDelay();
            std::size_t size = inputs.size();
            std::size_t workingMemorySize = this->workingMemorySize;
            std::vector<PlanStep> ops;

            for (std::size_t i = 0; i < inputs.size(); ++i)
            {
                PlanStep planStep;
                planStep.inputIndex = (fft_index_t)i;
                size_t outputIndex = (size + i - maxDelay) % size;
                planStep.outputIndex = outputs[outputIndex]->GetStorageIndex();

                std::size_t midPoint = _schedule.size() / 2;
                for (auto op : _schedule[i + midPoint])
                {
                    if (op->GetOpType() == FftOp::OpType::ButterflyOp)
                    {
                        planStep.ops.push_back(CompileOp((ButterflyOp *)(op)));
                    }
                }
                for (auto op : _schedule[i])
                {
                    if (op->GetOpType() == FftOp::OpType::ButterflyOp)
                    {
                        planStep.ops.push_back(CompileOp((ButterflyOp *)(op)));
                    }
                }
                ops.push_back(std::move(planStep));
            }
            std::vector<FftPlan::ConstantEntry> compiledConstants;
            for (auto &constant : constants)
            {
                ConstantOp *op = (ConstantOp *)(constant.get());
                compiledConstants.push_back(
                    FftPlan::ConstantEntry{op->GetStorageIndex(), op->GetValue()});
            }
            return std::make_shared<FftPlan>(maxDelay, workingMemorySize, std::move(ops), std::move(compiledConstants));
        }

        size_t Size() const { return inputs.size(); }
        void CheckForOverwrites()
        {
            // If maxDelay is more than 150% of Size(),
            // outut values get overrwritten by the final butterfly.
            // if (CalculateMaxDelay() >= Size() *3/ 2)
            // {
            //     throw std::logic_error("Can't schedule.");
            // }
            // problem: an op result for the next FFT frame overwrites data required by an op in the
            // currrent cycle.
            // this occurs if the schedule slot for an op minus the schedule slotof it's inputs is greater than N.
            for (auto &ops : _schedule)
            {
                for (auto op : ops)
                {
                    fft_index_t slot = op->GetEarliestAvailable();

                    fft_index_t dependentSlot = op->GetInput(0)->GetEarliestAvailable();
                    if (dependentSlot == CONSTANT_INDEX)
                    {
                        dependentSlot = op->GetInput(1)->GetEarliestAvailable();
                    }
                    else
                    {
                        fft_index_t t = op->GetInput(1)->GetEarliestAvailable();
                        if (t != CONSTANT_INDEX && t < dependentSlot)
                        {
                            dependentSlot = t;
                        }
                    }
                    // std::cout << "(" << slot - dependentSlot << ") ";
                    if (slot - dependentSlot > (fft_index_t)Size())
                    {
                        throw std::logic_error("Can't schedule.");
                    }
                }
            }
            // std::cout << std::endl;
        }
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
            std::size_t maxDelay = 0;
            for (std::size_t i = 0; i < outputs.size(); ++i)
            {
                std::size_t delay = outputs[i]->GetEarliestAvailable() - i;
                if (delay > maxDelay)
                    maxDelay = delay;
            }
            return maxDelay;
        }

    private:
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

        void AllocateMemory()
        {
            IndexAllocator allocator(_schedule.size());

            // pre-allocate indices for inputs.
            allocator.Allocate(inputs.size(), nullptr);
            // don't recycle memory for outputs.
            for (auto &output : outputs)
            {
                output->AddInputReference();
            }
            // allocate constants.
            for (auto &op : constants)
            {
                op->AllocateMemory(allocator);
            }
            // LeftOutputOp and RightOutputOp instances aren't visible in the schedule.
            // Call AllocateMemory() to copy the storage indices from the referenced
            // butterflyOps.
            // Allocate first to make sure we don't use a recycled slot.
            for (auto &output : outputs)
            {
                output->AllocateMemory(allocator);
            }
            for (std::size_t i = 0; i < _schedule.size(); ++i)
            {
                for (auto &op : _schedule[i])
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

        schedule_t _schedule;
        std::size_t maxOpsPerCycle = 2;
        std::size_t workingMemorySize = (std::size_t)-1;

        std::size_t GetOpCount(std::size_t slot)
        {
            std::size_t midPoint = _schedule.size() / 2;
            slot = slot % midPoint;
            return _schedule[slot].size() + _schedule[slot + midPoint].size();
        }
        std::size_t ScheduleOp(std::size_t slot, FftOp *op)
        {
            std::size_t initialSlot = slot;
            std::size_t schedSize = _schedule.size();
            while (true)
            {
                std::size_t currentOps = GetOpCount(slot);
                if (currentOps < maxOpsPerCycle)
                {
                    _schedule[slot % schedSize].push_back(op);
                    op->SetEarliestAvailable(slot);
                    return slot;
                }
                ++slot;
                if (slot % schedSize == initialSlot % schedSize)
                {
                    throw std::logic_error("Fft scheduling failed.");
                }
            }
        }
        void ScheduleOps()
        {
            // this->maxOpsPerCycle = this->maxOpsPerCycle*this->maxOpsPerCycle+this->maxOpsPerCycle;

            _schedule.resize(0);
            _schedule.resize(inputs.size() * 2);
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
                    slot = ScheduleOp(slot, op);
                    op->SetEarliestAvailable(slot);
                }
            }
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

BalancedFft::plan_ptr BalancedConvolutionSection::GetPlan(std::size_t size, std::size_t offset, std::vector<float> &data)
{
    plan_ptr plan;
    Builder builder;
    plan = builder.MakeConvolutionSection(size, offset, data);
    planCache[size] = plan;
    return plan;
}

BalancedFft::BalancedFft(std::size_t size, FftDirection direction)
{
    this->SetPlan(GetPlan(size, direction));
}
BalancedConvolutionSection::BalancedConvolutionSection(std::size_t size, size_t offset, std::vector<float> &data)
{
    this->SetPlan(GetPlan(size, offset, data));
}

void Implementation::FftPlan::PrintPlan()
{
    PrintPlan(std::cout);
}
void Implementation::FftPlan::PrintPlan(const std::string &fileName)
{
    std::ofstream o{fileName};
    if (!o)
    {
        throw std::logic_error("Can't open file for output.");
    }
    PrintPlan(o);
}
void Implementation::FftPlan::PrintPlan(std::ostream &output)
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
        output << "      output: " << step.outputIndex << endl;
        output << "      ops: [" << endl;
        for (auto &op : step.ops)
        {
            output << "        "
                   << op.in0
                   << "," << op.in1
                   << "->" << op.out;
#if DEBUG_OPS
            output << "  " << op.id.substr(0, std::min(op.id.length(), size_t(50)));
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
    this->workingMemory.resize(0);
    this->workingMemory.resize(plan->StorageSize());
    plan->InitializeConstants(this->workingMemory);
}

void BalancedConvolutionSection::SetPlan(plan_ptr plan)
{
    this->plan = plan;

    this->evenWorkingMemory.resize(0);
    this->evenWorkingMemory.resize(plan->StorageSize());
    plan->InitializeConstants(this->evenWorkingMemory);

    this->oddWorkingMemory.resize(0);
    this->oddWorkingMemory.resize(plan->StorageSize());
    plan->InitializeConstants(this->oddWorkingMemory);
    evenPlanIndex = plan->Size() / 2;
    oddPlanIndex = 0;
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
BalancedConvolution::BalancedConvolution(size_t size, std::vector<float> impulseResponse)
{
    constexpr size_t INITIAL_SECTION_SIZE = 64;

    if (size < INITIAL_SECTION_SIZE)
    {
        directConvolutionLength = size;
        delayLine.SetSize(directConvolutionLength + 1);
    }
    else
    {
        size_t sectionSize = INITIAL_SECTION_SIZE;
        size_t sectionDelay = BalancedConvolutionSection::GetSectionDelay(sectionSize);

        directConvolutionLength = sectionDelay;
        if (directConvolutionLength > size)
        {
            directConvolutionLength = size;
        }

        size_t sampleOffset = directConvolutionLength;

        while (sampleOffset < size)
        {
            size_t remaining = size - sampleOffset;
            while (remaining <= sectionSize / 2 && sectionSize > INITIAL_SECTION_SIZE)
            {
                sectionSize = sectionSize / 2;
                sectionDelay = BalancedConvolutionSection::GetSectionDelay(sectionSize);
            }

            size_t nextSectionDelay = BalancedConvolutionSection::GetSectionDelay(sectionSize * 2);
            if (sampleOffset > nextSectionDelay)
            {
                sectionSize *= 2;
                sectionDelay = nextSectionDelay;
            }
            std::cout << "sampleOffset: " << sampleOffset
                      << " SectionSize: " << sectionSize
                      << " sectionDelay: " << sectionDelay
                      << " input delay: " << (sampleOffset - sectionDelay)
                      << std::endl;
            sections.emplace_back(
                Section{
                    sampleOffset - sectionDelay,
                    BalancedConvolutionSection(
                        sectionSize,
                        sampleOffset,
                        impulseResponse)});
            sampleOffset += sectionSize;
        }
    }
    directImpulse.resize(directConvolutionLength);
    for (size_t i = 0; i < directConvolutionLength; ++i)
    {
        directImpulse[i] = impulseResponse[i];
    }
    size_t maxDelay = directConvolutionLength;
    for (auto &section : sections)
    {
        if (section.sampleDelay > maxDelay)
        {
            maxDelay = section.sampleDelay;
        }
    }
    delayLine.SetSize(maxDelay + 1);
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

void BalancedConvolution::DelayLine::SetSize(size_t size)
{
    size = NextPowerOf2(size);
    this->size_mask = size - 1;
    this->head = 0;
    this->storage.resize(0);
    this->storage.resize(size);
}

void BalancedFft::PrintPlan()
{
    this->plan->PrintPlan();
}
#define TEST_ASSERT(x) \
{\
    if (!(x))\
    {\
        throw std::logic_error(SS("Assert failed: " << #x));\
    }\
}


void Implementation::SlotUsageTest()
{
    {
        SlotUsage slotUsage(256);
        slotUsage.Add(0,10);
        TEST_ASSERT(slotUsage.Size() == 1);
        slotUsage.Add(11,12);
        TEST_ASSERT(slotUsage.Size() == 2);

        TEST_ASSERT(slotUsage.contains(11));
        TEST_ASSERT(!slotUsage.contains(12));
        TEST_ASSERT(!slotUsage.contains_any(10,11));
        TEST_ASSERT(!slotUsage.contains_any(10,10));
        TEST_ASSERT(slotUsage.contains_any(11,11));

        TEST_ASSERT(slotUsage.contains_any(11,13));
        TEST_ASSERT(slotUsage.contains_any(11,11));
        TEST_ASSERT(!slotUsage.contains_any(12,13));
        TEST_ASSERT(!slotUsage.contains_any(12,13));
        TEST_ASSERT(!slotUsage.contains_any(12,12));

    }
    {
        SlotUsage slotUsage(256);
        slotUsage.Add(255,256+10);
        TEST_ASSERT(slotUsage.Size() == 2);
        slotUsage.Add(10,10);
        TEST_ASSERT(slotUsage.Size() == 2);

        slotUsage.Add(10,12);
        TEST_ASSERT(slotUsage.Size() == 2);

        TEST_ASSERT(slotUsage.contains(9));
        TEST_ASSERT(slotUsage.contains(10));
        TEST_ASSERT(!slotUsage.contains(12));
        TEST_ASSERT(slotUsage.contains_any(10,11));
        TEST_ASSERT(slotUsage.contains_any(10,10));
        TEST_ASSERT(slotUsage.contains_any(11,15));

        TEST_ASSERT(slotUsage.contains_any(11,13));
        TEST_ASSERT(slotUsage.contains_any(11,11));
        TEST_ASSERT(!slotUsage.contains_any(12,13));
    }
    {
        SlotUsage slotUsage(256);
        slotUsage.Add(0,10);
        TEST_ASSERT(slotUsage.Size() == 1);
        slotUsage.Add(12,12);
        TEST_ASSERT(slotUsage.Size() == 2);

        TEST_ASSERT(slotUsage.contains(9));
        TEST_ASSERT(!slotUsage.contains(10));
        TEST_ASSERT(!slotUsage.contains(12));
        TEST_ASSERT(!slotUsage.contains_any(11,12)); // yes we can re-use the slot.

        TEST_ASSERT(!slotUsage.contains_any(12,13)); // yes we can re-use the slot.
        TEST_ASSERT(!slotUsage.contains_any(13,14)); // yes we can re-use the slot.
        TEST_ASSERT(slotUsage.contains_any(11,13));
        TEST_ASSERT(!slotUsage.contains_any(12,12));

        TEST_ASSERT(!slotUsage.contains_any(13,13));

        slotUsage.Add(13,13);
        TEST_ASSERT(slotUsage.Size() == 3);
        slotUsage.Add(13,14);
        TEST_ASSERT(slotUsage.Size() == 3);

        slotUsage.Add(17,17);
        TEST_ASSERT(slotUsage.Size() == 4);

        slotUsage.Add(16,17);
        TEST_ASSERT(slotUsage.Size() == 4);

    }

}