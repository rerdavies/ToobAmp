#include "Fft.hpp"
#include "StagedFft.hpp"
#include <cstddef>
#include <cassert>
#include "../TestAssert.hpp"
#include <iostream>
#include <numbers>
#include <random>


using namespace LsNumerics;
using namespace std;

template <typename FftType>
static void fftTest(FftType& fft)
{
    size_t N = fft.GetSize();
    std::vector<std::complex<double>> input;
    input.resize(N);
    for (size_t i = 0; i < N; ++i)
    {
        input[i] = i+1;
    }

    std::vector<std::complex<double>> forwardResult;
    forwardResult.resize(N);
    std::vector<std::complex<double>> inverse;
    inverse.resize(N);
    
    fft.Forward(input,forwardResult);
    fft.Backward(forwardResult,inverse);

    for (size_t i = 0; i < inverse.size(); ++i)
    {
        TEST_ASSERT(std::abs(inverse[i].real()-input[i].real()) < 1E-4);
    }

    
    // Check pure sine waves at integer frequencies.

    for (size_t f = 1; f < N/2; f *= 2)
    {
        for (size_t i = 0; i < N; ++i)
        {
            input[i] = std::sin(2*std::numbers::pi/N*f*i);
        }
        fft.Forward(input,forwardResult);

        for (size_t i = 0; i < N/2; ++i)
        {
            double t = std::abs(forwardResult[i]);
            if (i == f)
            {
                TEST_ASSERT(std::abs(t-std::sqrt(double(N))/2) < 1E-7);
            } else {
                TEST_ASSERT(std::abs(t) < 1E-7);
            }
        }
    }
    // check in-place fft.
    std::vector<std::complex<double>> inPlaceBuffer;
    inPlaceBuffer.reserve(input.size());
    inPlaceBuffer.insert(inPlaceBuffer.begin(),input.begin(),input.end());

    fft.Forward(inPlaceBuffer,inPlaceBuffer);
    for (size_t i = 0; i < inPlaceBuffer.size(); ++i)
    {
        TEST_ASSERT(inPlaceBuffer[i] == forwardResult[i]);
    }
    // check random values round-trip.

    static std::mt19937 randomDevice;

    static std::uniform_real_distribution<float> distribution{-1.0f,1.0f};



    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = distribution(randomDevice);
    }
    std::vector<std::complex<double>> result;
    result.resize(input.size());
    fft.Forward(input,forwardResult);
    fft.Backward(forwardResult,result);

    for (size_t i = 0; i < input.size(); ++i)
    {
        float error = std::abs(input[i]-result[i].real());
        TEST_ASSERT(error < 1E-7);
    }


}

extern void TestFftShuffle();

int main(int argc, const char**argv)
{
    std::cout << "== FftTest ====" << std::endl;


    {
        StagedFft stagedFft(64*1024);
        fftTest<LsNumerics::StagedFft>(stagedFft);
    }

    try {
    for (size_t n = 2; n < 512*1204; n *= 2)
    {
        std::cout << "size = " << n << std::endl;
        StagedFft stagedFft(n);
        fftTest<LsNumerics::StagedFft>(stagedFft);

        Fft fft(n);
        fftTest<Fft>(fft);
    }
    } catch (const std::exception&e)
    {
        std::cout << "FftTest failed: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;

}