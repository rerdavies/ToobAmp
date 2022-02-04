#include "dft.h"
#include <cstddef>
#include <cassert>


using namespace LsNumerics;
using namespace std;

template <int N>
static void fftTest()
{
    Dft<double> fft { N};
    std::vector<std::complex<double>> input;
    input.resize(N);
    for (size_t i = 0; i < N; ++i)
    {
        input[i] = i+1;
    }

    std::vector<std::complex<double>> forwardResult = fft.forward(input);
    std::vector<std::complex<double>> inverse = fft.backward(forwardResult);

    for (size_t i = 0; i < inverse.size(); ++i)
    {
        assert(std::abs(inverse[i]-input[i]) < 1E-7);
    }

    
    // Check pure sine waves at integer frequencies.
    for (int f = 1; f < N/2; f *= 2)
    {
        for (int i = 0; i < N; ++i)
        {
            input[i] = std::sin(2*Pi/N*f*i);
        }
        forwardResult = fft.forward(input);

        for (int i = 0; i < N/2; ++i)
        {
            double t = std::abs(forwardResult[i]);
            if (i == f)
            {
                assert(std::abs(t-std::sqrt(N)/2) < 1E-7);
            } else {
                assert(std::abs(t) < 1E-7);
            }
        }
    }


}

int main(int argc, const char**argv)
{

    fftTest<4>();
    fftTest<32768>();
    return 0;

}