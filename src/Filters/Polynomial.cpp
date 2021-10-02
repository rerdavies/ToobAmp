#include "Polynomial.h"
#include <cmath>

using namespace TwoPlay;

const Polynomial Polynomial::ONE(1.0);
const Polynomial Polynomial::ZERO;


static void ReduceVector(std::vector<double>*vec)
{
    size_t ix = vec->size();
    while (ix != 0 && vec->at(ix-1) == 0)
    {
        --ix;
    }
    if (ix != vec->size())
    {
        vec->resize(ix);
    }
}
Polynomial Polynomial::operator+(const Polynomial & other)
{
    std::vector<double> result(std::max(this->values.size(),other.values.size()));
    int iMin = std::min(values.size(),other.values.size());
    for (int i = 0; i < iMin; ++i)
    {
        values.push_back(values[i] + other.values[i]);
    }
    if (values.size() > other.values.size())
    {
        for (std::size_t i = iMin; i < values.size(); ++i)
        {
            result.push_back(values[i]);
        }
    } else {
        for (std::size_t i = iMin; i < other.values.size(); ++i)
        {
            result.push_back(other.values[i]);
        }
    }
    ReduceVector(&result);
    return Polynomial(result);

}
Polynomial Polynomial::operator-(const Polynomial & other)
{
    std::vector<double> result(std::max(this->values.size(),other.values.size()));
    int iMin = std::min(values.size(),other.values.size());
    for (int i = 0; i < iMin; ++i)
    {
        values.push_back(values[i] - other.values[i]);
    }
    if (values.size() > other.values.size())
    {
        for (std::size_t i = iMin; i < values.size(); ++i)
        {
            result.push_back(values[i]);
        }
    } else {
        for (std::size_t i = iMin; i < other.values.size(); ++i)
        {
            result.push_back(-other.values[i]);
        }
    }
    ReduceVector(&result);
    return Polynomial(result);

}
Polynomial Polynomial::operator*(const Polynomial & other) {
    std::vector<double> result;
    result.resize(values.size()+other.values.size());

    for (size_t i = 0; i < values.size(); ++i)
    {
        for (size_t j = 0; j < other.values.size(); ++j)
        {
            result[i+j] += values[i]*other.values[j];
        }
    }
    ReduceVector(&result);

    return Polynomial(result);
}

