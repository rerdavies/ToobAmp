#pragma once

#include "std.h"
#include <memory.h>
#include "ToobMath.h"

namespace TwoPlay {
    class IDelay {
    private:
        float*buffer = NULL;
        int32_t ixMask = 0;
        int32_t head = 0;
        int32_t delay = 1;

    public:
        ~IDelay() {
            free(buffer);
            buffer = NULL;
        }
        void SetMaxDelay(int samples)
        {
            samples = NextPowerOfTwo(samples+1);
            if (samples-1 != ixMask)
            {
                if (buffer != NULL) free(buffer);
                buffer = NULL;
                buffer = (float*)calloc(samples,sizeof(float));
                ixMask = samples-1;
                Reset();
            }
        }
        void Reset()
        {
            if (buffer != NULL)
            {
                memset(buffer,0,ixMask+1);
            }
            head = 0;
        }
        void SetDelay(int32_t samples)
        {
            this->delay = samples;
        }
        int32_t GetDelay() const { return this->delay; }

        inline float Tick(float value)
        {
            int ix = this->head = (head-1) & ixMask;
            buffer[ix] = value;
            return buffer[(ix + delay) & ixMask];
        }
    };
}