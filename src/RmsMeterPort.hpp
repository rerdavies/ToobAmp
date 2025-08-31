/*
MIT License

Copyright (c) 2025 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include "LsNumerics/LsMath.hpp"
#include <cstddef>

namespace toob {

    class RmsMeterPort {
    public:
        void SetData(void*data)
        {
            this->data = (float*)data;
            if (this->data)
            {
                *this->data = 0;
            }
        }
        void SetSampleRate(double sampleRate, float minDb = -120) {
            this->minDb = minDb;
            size_t bufferSize = (size_t)(1/10.343253 * sampleRate);
            meanSquareBuffer.resize(bufferSize);

            updateRate = (size_t)(sampleRate/15.0);
            
            Reset();
        }
        void Reset() 
        {
            std::fill(meanSquareBuffer.begin(),meanSquareBuffer.end(),0);
            runningMeanSquare = 0;
            meanSquareBufferIndex = 0;
            meanSquarePeak = 0;
        }
        void Tick(float*input, size_t count) {
            size_t ix = meanSquareBufferIndex;
            for (size_t i = 0; i < count; ++i)
            {
                float value = input[i];
                value *= value;
                runningMeanSquare += value- meanSquareBuffer[ix];
                meanSquareBuffer[ix] = value;
                if (runningMeanSquare > meanSquarePeak) 
                {
                    meanSquarePeak = runningMeanSquare;
                }

                ++ix;
                if (ix == meanSquareBuffer.size()) 
                {
                    ix = 0;
                }
            }
            meanSquareBufferIndex = ix;

            updateIndex += count;
            if (updateIndex > updateRate) 
            {
                updateIndex -= updateRate;
                double rms = std::sqrt(meanSquarePeak/meanSquareBuffer.size());
                double db;
                if (rms == 0)
                {
                    db = minDb;
                } else {
                    db = LsNumerics::Af2Db(rms);
                    if (db < minDb+1) 
                    {
                        db = minDb+1;
                    }
                } 
                
                *data = (float)db;
                meanSquarePeak = 0;
            }
        }

    private:
        double runningMeanSquare = 0;
        size_t meanSquareBufferIndex = 0;
        std::vector<float> meanSquareBuffer;
        double meanSquarePeak = 0;
        float minDb;

        float *data = nullptr;
        size_t updateRate;
        size_t updateIndex;
    };

};