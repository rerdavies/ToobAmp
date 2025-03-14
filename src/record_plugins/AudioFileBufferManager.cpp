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

#include "AudioFileBufferManager.hpp"
#include <stdexcept>
#include <iostream>

using namespace toob;

AudioFileBuffer::AudioFileBuffer(size_t channels, size_t bufferSize)
    : bufferSize_(bufferSize)
{
    data_.resize(channels);
    for (size_t i = 0; i < channels; ++i)
    {
        data_[i].resize(bufferSize);
    }
    refCount = 1;
}

AudioFileBuffer::~AudioFileBuffer()
{
}

AudioFileBuffer::ptr AudioFileBuffer::Create(size_t channels, size_t bufferSize)
{
    return AudioFileBuffer::ptr(new AudioFileBuffer(channels, bufferSize));
}

AudioFileBufferPool::AudioFileBufferPool(size_t channels, size_t bufferSize, size_t reserve)
    : channels(channels), bufferSize(bufferSize)
{
    Reserve(reserve);
}

static void DBG_ASSERT(const std::string &&message)
{
#ifdef DEBUG
    throw std::runtime_error(message);
#else
    std::cerr << message << std::endl;
#endif
}

AudioFileBufferPool::~AudioFileBufferPool()
{
    try
    {
        Trim(0);
        TestPoolCount(0);
        if (this->freeList != nullptr)
        {
            DBG_ASSERT("AudioFileBufferPool::~AudioFileBufferPool: freeList not empty");
        }
        if (this->allocatedCount != 0)
        {
            DBG_ASSERT("AudioFileBufferPool::~AudioFileBufferPool: Elements leaked.");
        }
    }
    catch (const std::exception &e)
    {
// do what we can to report an error since we can't throw.
        std::cout << "Warning: " << e.what() << std::endl;
    }
}

void AudioFileBufferPool::Reserve(size_t count)
{
    while (pooledCount < count)
    {
        AudioFileBuffer *buffer = new AudioFileBuffer(channels, bufferSize);
        ++allocatedCount;
        PutBuffer(buffer);
    }
}
void AudioFileBufferPool::Trim(size_t count)
{
    while (pooledCount > count)
    {
        auto buffer = TakeBuffer();
        size_t count = buffer->Release();
        if (count != 0)
        {
            throw std::runtime_error("AudioFileBufferPool::Trim: buffer has non-zero ref count");
        }
        --allocatedCount;
    }
}

void AudioFileBufferPool::PutBuffer(AudioFileBuffer *buffer)
{
    if (buffer->refCount.load() != 1)
    {
        throw std::runtime_error("AudioFileBufferPool::Trim: buffer has invalid ref count");
    }

    AudioFileBuffer *current = freeList.load(std::memory_order_relaxed);
    buffer->next = current;
    while (!freeList.compare_exchange_weak(buffer->next, buffer, std::memory_order_release, std::memory_order_relaxed))
    {
        /**/;
    }
    ++pooledCount;
}

AudioFileBuffer *AudioFileBufferPool::TakeBuffer()
{
    AudioFileBuffer *current = freeList.load(std::memory_order_relaxed);
    while (current)
    {
        AudioFileBuffer *next = current->next;
        if (freeList.compare_exchange_weak(current, next, std::memory_order_acquire, std::memory_order_relaxed))
        {
            pooledCount--;
            return current;
        }
    }
    // No pooled buffers. create a new one.
    ++allocatedCount;
    return new AudioFileBuffer(channels, bufferSize);
}

void AudioFileBufferPool::TestPoolCount(size_t expected)
{
    if (pooledCount != expected)
    {
        throw std::runtime_error("AudioFileBufferPool::TestPoolCount: pool count mismatch");
    }

    auto t = this->freeList.load();
    size_t n = 0;

    while (t)
    {
        t = t->next;
        ++n;
    }
    if (n != expected)
    {
        throw std::runtime_error("AudioFileBufferPool::TestPoolCount: free list count mismatch");
    }
}
