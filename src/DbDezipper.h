#pragma once

#include "std.h"
#include "ToobMath.h"

namespace TwoPlay {
    class DbDezipper {
    private:
        float targetDb = -96;
        float currentDb = -96;
        float targetX = 0;
        float x = 0;
        float dx = 0;
        int32_t count = -1;
        float dbPerSegment;
        void NextSegment();
    public:
        void SetSampleRate(double rate);
        void Reset()
        {
            x = targetX = 0;
            dx = 0;
            currentDb = -96;
            targetDb = -96;
            count = -1;
        }

        void SetTarget(float db)
        {
            if (db < -96) db  = -96;
            if (db != targetDb) {
                targetDb = db;
                count = 0;
            }
        }

        inline float Tick() { 
            if (count >= 0)
            {
                if (count-- <= 0)
                {
                    NextSegment();
                }
                float result = x;
                x += dx;

                return result;
            }
            return x;
        }
    };
}