#include "FilterCoefficients.h"

using namespace TwoPlay;

void FilterCoefficients::Copy(const FilterCoefficients&other)
{
    delete[] a; delete[] b;
    this->length = other.length;
    a = new double[length];
    b = new double[length];
    for (size_t i = 0; i < length; ++i)
    {
        this->a[i] = other.a[i];
        this->b[i] = other.b[i];
    }
}
