/*
 *   Copyright (c) 2022 Robin E. R. Davies
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

#include <vector>
#include <cmath>

namespace toob {
    class FilterResponse {
    private:
        std::vector<float> frequencies;
        std::vector<float> responses;
        const int MIN_FREQUENCY = 30;
        const int MAX_FREQUENCY = 22050;

        float CalculateFrequency(int n)
        {
            double logMin =std::log(MIN_FREQUENCY);
            double logMax = std::log(MAX_FREQUENCY);
            double logN = (logMax-logMin)*n/RESPONSE_BINS+logMin;
            return std::exp(logN);
        }
        bool requested = false;
    public:
        int RESPONSE_BINS = 64;

        FilterResponse(int responseBins = 64)
        {
            this->RESPONSE_BINS = responseBins;
            frequencies.resize(RESPONSE_BINS);
            responses.resize(RESPONSE_BINS);
            for (int i = 0; i < RESPONSE_BINS; ++i)
            {
                frequencies[i] = CalculateFrequency(i);
            }
        }

        float GetFrequency(int n) { return frequencies[n];}
        void SetResponse(int n,float response)
        {
            responses[n] = response;
        }
        float GetResponse(int n) { return responses[n];}
        void SetRequested(bool value) { requested = value;}
        bool GetRequested() { return requested; }

    };
}