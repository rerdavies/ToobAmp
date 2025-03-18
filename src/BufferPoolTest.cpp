// Copyright (c) 2025 Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "record_plugins/AudioFileBufferManager.hpp"
#include <iostream>
#include <thread>
#include <array>
using namespace std;


using namespace toob;

static void CheckPoolBuffers(AudioFileBufferPool &pool, size_t n) 
{

}

void BasicOps() {
    {
        AudioFileBufferPool pool(1,1024,6);

        CheckPoolBuffers(pool,6);

        if (pool.AllocationCount() != 6) {
            throw std::runtime_error("Allocation count mismatch");
        }

        auto buffer = pool.TakeBuffer();
        CheckPoolBuffers(pool,5);
        if (pool.AllocationCount() != 6) {
            throw std::runtime_error("Allocation count mismatch");
        }

        pool.PutBuffer(buffer);
        CheckPoolBuffers(pool,6);

        if (pool.AllocationCount() != 6) {
            throw std::runtime_error("Allocation count mismatch");
        }

        pool.Trim(0);
        CheckPoolBuffers(pool,0);
        if (pool.AllocationCount() != 0) {
            throw std::runtime_error("Allocation count mismatch");
        }


    }
}


void MultiThreadedTest() {

    AudioFileBufferPool pool(1,1024,6);

    CheckPoolBuffers(pool,6);

    std::thread t1([&](){
        std::array <AudioFileBuffer*,7> buffers;

        for (size_t i = 0; i < 10000*9; ++i) {
            for (size_t j = 0; j < buffers.size(); ++j) {
                buffers[j] = pool.TakeBuffer();
            }
            for (size_t j = 0; j < buffers.size(); ++j) {
                pool.PutBuffer(buffers[j]);
            }
        }
    });


    std::thread t2([&](){
        std::array <AudioFileBuffer*,12> buffers;

        for (size_t i = 0; i < 10000*3; ++i) {
            for (size_t j = 0; j < buffers.size(); ++j) {
                buffers[j] = pool.TakeBuffer();
            }
            for (size_t j = 0; j < buffers.size(); ++j) {
                pool.PutBuffer(buffers[j]);
            }
        }
    });

    t1.join();
    t2.join();

    pool.Trim(0);
    CheckPoolBuffers(pool,0);

}


int main(void) {

    try {
        BasicOps();


        MultiThreadedTest();

    } catch (const std::exception&e) 
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }




    return EXIT_SUCCESS;
}