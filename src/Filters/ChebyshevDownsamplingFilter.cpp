#include "ChebyshevDownsamplingFilter.h"
#include <cmath>
#include <exception>

using namespace TwoPlay;


static double a2db(double amplitude)
{
    return std::log10(amplitude)*20;
}

void ChebyshevDownsamplingFilter::Design(double samplingFrequency, 
    double dbRipple, double cutoffFrequency,
    double bandstopDb, double bandstopFrequency)
{
    for (int order = 4; order < 20; ++order)
    {
        filter.setup(order,samplingFrequency,cutoffFrequency,dbRipple);
        Iir::complex_t response = filter.response(bandstopFrequency/samplingFrequency);    

        double db = a2db(std::abs(response));
        if (db < bandstopDb) return;
    }
    throw std::invalid_argument("Downsampling filter design failed.");

}
