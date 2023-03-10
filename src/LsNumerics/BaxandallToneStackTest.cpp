#include "BaxandallToneStack.hpp"
#include <functional>
#include <assert.h>
#include "LsMath.hpp"

using namespace LsNumerics;
using namespace std;

void testResponse(BaxandallToneStack &toneStack, const function<void(double, double)> &callback)
{
    for (int i = 0; i < 1000; ++i)
    {
        double f = i * toneStack.GetSampleRate() / 1000;
        double responseDb = Af2Db(toneStack.GetFrequencyResponse(f));
        callback(f, responseDb);
    }
}

static void checkDesignResponse(BaxandallToneStack &toneStack, double frequency)
{
#ifndef NDEBUG
    double zResponse = LsNumerics::Af2Db(toneStack.GetFrequencyResponse(frequency));
    double sResponse = LsNumerics::Af2Db(toneStack.GetDesignFrequencyResponse(frequency));
    assert(std::abs(zResponse - sResponse) < 1E-5);
#endif
}

int main(int, char **)
{
    BaxandallToneStack toneStack;
    toneStack.SetSampleRate(48000);
    toneStack.Design(0.5, 0.5);

    double min = 1E30;
    double max = -1E30;

    checkDesignResponse(toneStack, 0);
    checkDesignResponse(toneStack, toneStack.Fc);
    testResponse(toneStack, [&min, &max](double frequency, double responseDb) -> void
                 {
        if (responseDb < min) min = responseDb;
        if (responseDb > max) max = responseDb; });
    assert(max - min < 6);
    return 0;
}