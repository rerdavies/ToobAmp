#pragma once
#include "AudioFilter2.h"
#include "../LsNumerics/LsMath.hpp"
#include <cmath>

namespace TwoPlay {
    using namespace LsNumerics;

    class ShelvingLowCutFilter2: public AudioFilter2 {
    private:

        float lowCutDb;
        bool disabled;
        float cutoffFrequency = 4000;
    public:
        ShelvingLowCutFilter2()
        {
            SetLowCutDb(0);
        }

        void SetLowCutDb(float db)
        {
            this->lowCutDb = db;
            if (db > 0) db = -db;
            if (db != 0)
            {
                disabled = false;
                float g = Db2Af(db);

                prototype.b[0] = g;
                prototype.b[1] = std::sqrt(g/2);
                prototype.b[2] = 1;
                prototype.a[0] = prototype.a[2] = 1;
                prototype.a[1] = std::sqrt(2);
                ShelvingLowCutFilter2::SetCutoffFrequency(this->cutoffFrequency);
            } else {
                disabled = true;
                this->zTransformCoefficients.Disable();
            }
            
        }
        virtual void SetCutoffFrequency(float frequency)
        {
            this->cutoffFrequency = frequency;
            if (!disabled)
            {
                AudioFilter2::SetCutoffFrequency(frequency);
            }
        }

    };

};