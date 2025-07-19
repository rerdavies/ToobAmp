/*
 *   Copyright (c) 2025 Robin E. R. Davies
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

#include <atomic>
#include <vector>
#include <cstdint>

namespace toob
{

    // Provide zero-allocation buffers and buffer management for file data on the realtime thread.

    class ToobObject
    {
    protected:
        ToobObject() {}
        virtual ~ToobObject() {}

    public:
        size_t AddRef();
        size_t Release();

    private:
        std::atomic<uint64_t> refCount = 1;
    };

    // like std::shared_ptr, but provides attach and deatch methods.
    template <typename T> 
    class ToobPtr
    {
    public:
        ToobPtr() : ptr(nullptr) {}
        ToobPtr(T *ptr) : ptr(ptr) {}
        ToobPtr(const ToobPtr &other) : ptr(other.ptr)
        {
            if (ptr)
                ptr->AddRef();
        }
        ToobPtr(ToobPtr &&other)
        {
            std::swap(ptr, other.ptr);
        }
        ~ToobPtr()
        {
            if (ptr)
                ptr->Release();
        }
        ToobPtr &operator=(const ToobPtr &other)
        {
            if (ptr)
                ptr->Release();
            ptr = other.ptr;
            if (ptr)
                ptr->AddRef();
            return *this;
        }
        ToobPtr &operator=(ToobPtr &&other)
        {
            std::swap(ptr, other.ptr);
            return *this;
        }
        operator bool() const { return ptr != nullptr; }

        T *detach()
        {
            T *result = ptr;
            ptr = nullptr;
            return result;
        }
        void attach(T *newPtr)
        {
            if (ptr)
                ptr->Release();
            ptr = newPtr;
        }
        T *operator->() { return ptr; }
        const T *operator->() const { return ptr; }
        T *Get() { return ptr; }
        const T *Get() const { return ptr; }

    private:
        T *ptr;
    };

    class AudioFileBuffer : public ToobObject
    {
    private:
        AudioFileBuffer(size_t channels, size_t bufferSize);
        ~AudioFileBuffer();

    public:
        using ptr = ToobPtr<AudioFileBuffer>;
        static ptr Create(size_t channels, size_t bufferSize);

        size_t GetChannelCount() const { return data_.size(); }
        size_t GetBufferSize() const { return bufferSize_; }
        void SetBufferSize(size_t size) { 
            bufferSize_ = size; 
        }
        void ResetBufferSize() {
            this->bufferSize_ = data_[0].size();
        }
        float *GetChannel(size_t channel) { return data_[channel].data(); }
        const float *GetChannel(size_t channel) const { return data_[channel].data(); }

    private:
        friend class AudioFileBufferPool;
        AudioFileBuffer* next = nullptr;
        std::atomic<uint64_t> refCount;
        size_t bufferSize_;
        std::vector<std::vector<float>> data_;
    };

    class AudioFileBufferPool {
    public:
        AudioFileBufferPool(size_t channels, size_t bufferSize, size_t reserve = 6);
        virtual ~AudioFileBufferPool();


        void Reserve(size_t count);
        void Trim(size_t count);

        AudioFileBuffer* TakeBuffer();
        void PutBuffer(AudioFileBuffer *buffer);

        void TestPoolCount(size_t expected);
        size_t AllocationCount() const { return allocatedCount; }

        size_t GetBufferSize() { return bufferSize; }
        size_t GetChannels() { return channels; }

    private:
        size_t channels;
        size_t bufferSize;
        std::atomic<size_t> pooledCount { 0};
        std::atomic<size_t> allocatedCount {0};
        std::atomic<AudioFileBuffer*> freeList;
    
    };

    /////////////////////////////////
    // Inline methods
    /////////////////////////////////
    inline size_t ToobObject::AddRef() { return ++refCount; }

    inline size_t ToobObject::Release()
    {
        size_t result = --refCount;
        if (result == 0)
        {
            delete this;
        }
        return result;
    }

};
