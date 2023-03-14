#include "Fft.hpp"
#include "StagedFft.hpp"
#include <cstddef>
#include <cassert>
#include "../TestAssert.hpp"
#include <iostream>
#include <numbers>


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
        TEST_ASSERT(std::abs(inverse[i]-input[i]) < 1E-7);
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


}

int main(int argc, const char**argv)
{
    std::cout << "== FftTest ====" << std::endl;

    try {
    for (size_t n: { 
        4,8,16, 128,256,512,2048,4096,32*1024})
    {
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