/*
 *   Copyright (c) 2021 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#pragma once

#include "InputPort.h"
#include "GainStage.h"
#include "DbDezipper.h"
#include "Filters/LowPassFilter.h"
#include "Filters/AudioFilter2.h"
#include "lv2/core/lv2.h"
#include "lv2/log/logger.h"
#include "lv2/uri-map/uri-map.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/worker/worker.h"
#include "lv2/patch/patch.h"
#include "lv2/parameters/parameters.h"
#include "lv2/units/units.h"
#include "FilterResponse.h"
#include <string>

#include "Lv2Plugin.h"

#include <cmath>

#define RESPONSE_CURVE_URI "http://two-play.com/ToobAmp/ResponseCurve"
#define RESPONSE_CURVE__instanceId_URI RESPONSE_CURVE_URI "#instanceId"
#define RESPONSE_CURVE__data_URI RESPONSE_CURVE_URI "#data"

namespace TwoPlay 
{

    class GainSection {
        private:
            static FilterCoefficients2 HIPASS_PROTOTYPE;

            GainStage gain;
            LowPassFilter lpFilter;
            AudioFilter2 hpFilter;
            DbDezipper trimVolume;
            float peakMax;
            float peakMin;
            struct  GainStageUris {
                void Map(Lv2Plugin* plugin)
			    {
				    ResponseCurve = plugin->MapURI(RESPONSE_CURVE_URI);
                    responseCurve_instanceId = plugin->MapURI(RESPONSE_CURVE__instanceId_URI);
                    responseCurve_data = plugin->MapURI(RESPONSE_CURVE__data_URI);
                    atom_Float = plugin->MapURI(LV2_ATOM__Float);
                    patch_Set = plugin->MapURI(LV2_PATCH__Set);
                    atom_float = plugin->MapURI(LV2_ATOM__Float);
                    patch_value = plugin->MapURI(LV2_PATCH__value);
                    patch_property = plugin->MapURI(LV2_PATCH__property);
                }

                LV2_URID ResponseCurve;
                LV2_URID responseCurve_instanceId;
                LV2_URID responseCurve_data;
                LV2_URID atom_Float;
                LV2_URID patch_Set;
                LV2_URID atom_float;
                LV2_URID patch_value;
                LV2_URID patch_property;


            };
            GainStageUris gainStageUris;
            void UpdateShape();
        public:
            GainSection();

            void WriteShapeCurve(
                LV2_Atom_Forge       *forge,
                LV2_URID              properyUri
            );

            bool Enable = true;
            RangedDbInputPort Trim = RangedDbInputPort(-20.0f, 20.0f);
            RangedInputPort Gain = RangedInputPort(0.0f,1.0f);
            RangedInputPort LoCut = RangedInputPort(30.0f, 300.0f);
            RangedInputPort HiCut = RangedInputPort(1000.0f, 19000.0f);
            RangedInputPort Bias = RangedInputPort(-2,2);
            SteppedInputPort Shape = SteppedInputPort(0,2);
            
            void InitUris(Lv2Plugin*pPlugin) {
                gainStageUris.Map(pPlugin);
            }

            void SetSampleRate(double rate);
            void Reset();
            void UpdateControls(
                );

            float GetVu() {
                float tMin = peakMin;
                float tMax = peakMax;
                peakMin = 0;
                peakMax = 0;
                return std::max(tMax,-tMin);
            }
            float GetPeakMax()
            {
                float tMax = peakMax;
                return tMax;
            }
            float GetPeakMin()
            {
                float tMin = peakMin;
                return tMin;
            }
            float GetPeakOutMax()
            {
                return this->gain.GainFn(peakMax);
            }
            float GetPeakOutMin()
            {
                return this->gain.GainFn(peakMin);
            }

            void ResetPeak()
            {
                peakMin = 0;
                peakMax = 0;
            }

            inline float TickSupersampled(float value) { 
                if (!Enable) return value;
                value *= trimVolume.Tick();
                if (value > peakMax) peakMax = value;
                if (value < peakMin) peakMin = value;
                float x = 
                    gain.TickSupersampled( 
                        lpFilter.Tick(
                            hpFilter.Tick(
                                value
                            )));
                return Undenormalize(x);
            }
            inline float Tick(float value) { 
                if (!Enable) return value;
                value *= trimVolume.Tick();
                if (value > peakMax) peakMax = value;
                if (value < peakMin) peakMin = value;

                float x = 
                    gain.Tick( 
                        lpFilter.Tick(
                            hpFilter.Tick(
                                value
                            )));
                return Undenormalize(x);
            }

    };
}