#pragma once

#include <vector>
#include <cmath>

namespace TwoPlay {
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