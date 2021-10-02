#pragma once

#include "../std.h"
#include "AudioFilter3.h"

namespace TwoPlay {

    class ToneStackFilter : public AudioFilter3 {

    private:
        void UpdateFilter();
        void BilinearTransform(float frequency, const FilterCoefficients3& prototype, FilterCoefficients3* result);
    public:
        RangedInputPort Bass =RangedInputPort(0,1);
		RangedInputPort Mid =RangedInputPort(0,1);
		RangedInputPort Treble =RangedInputPort(0,1);
		RangedInputPort AmpModel =RangedInputPort(0,1);

    public:
        bool UpdateControls()
        {
            if (Bass.HasChanged() || Mid.HasChanged() || Treble.HasChanged() || AmpModel.HasChanged())
            {
                UpdateFilter();
                return true;
            }
            return false;
        }




    };

}