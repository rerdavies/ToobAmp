#pragma once
#include <vector>

namespace TwoPlay {
    class Polynomial {
    public:
        static const Polynomial ONE;
        static const Polynomial ZERO;

        std::vector<double> values;

        Polynomial() {

        }
        Polynomial(int length, double*values)
        :values(length)
        {
            
            for (int i = 0; i < length; ++i)
            {
                this->values.push_back(values[i]);
            }
        }
        Polynomial(std::vector<double> values)
        {
            this->values = values;
        }

        Polynomial(double v0)
        {
            values.push_back(v0);
        }

        Polynomial(double v0, double v1)
        {
            values.push_back(v0);
            values.push_back(v1);
        }
        double & operator[](int index) {
            return values.at(index);
        }

        void Resize(size_t size) { values.resize(size);}

        Polynomial operator+(const Polynomial & other);
        Polynomial operator-(const Polynomial & other);
        Polynomial operator*(const Polynomial & other);
        Polynomial& operator=(const Polynomial & other)
        {
            this->values = other.values;
            return *this;
        }

    };
}