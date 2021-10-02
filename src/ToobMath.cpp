#include "ToobMath.h"


using namespace TwoPlay;


float MathInternal::log10 = std::log(10.0f);

uint32_t TwoPlay::NextPowerOfTwo(uint32_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value+1;
}
