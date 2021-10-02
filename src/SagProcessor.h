#pragma once
#include "Filters/LowPassFilter.h"
#include "InputPort.h"
#include <cmath>
#include "ToobMath.h"

namespace TwoPlay{
    class SagProcessor {
    private:
        LowPassFilter powerFilter;
        float currentPower = 0.0f;
        float currentSag = 1;
        float currentSagD = 1;
        float sagAf = 1;
        float sagDAf = 1;
    public:
        RangedInputPort Sag;
        RangedInputPort SagD;
        RangedInputPort SagF;
        
    public:
        SagProcessor()
        :Sag(0,1),
         SagD(0,1),
         SagF(5.0f,25.0f)
         {

         }
        void SetSampleRate(double rate) 
        {
            powerFilter.SetSampleRate(rate);
            powerFilter.SetCutoffFrequency(13.0);
        }

        void Reset()
        {
            powerFilter.Reset();
            currentSagD = 1.0f;
            currentSag = 1.0f;
        }

        void UpdateControls()
        {
            if (Sag.HasChanged())
            {
                float val = Sag.GetValue();
                float dbSag = val * 30;
                sagAf = Db2Af(dbSag);
            }
            if (SagD.HasChanged())
            {
                float val = SagD.GetValue();
                float dbSagD = val*30;
                sagDAf = Db2Af(dbSagD); 
            }
            if (SagF.HasChanged())
            {
                float val = SagF.GetValue();
                powerFilter.SetCutoffFrequency(val);
            }
        }
        inline float GetSagDValue() 
        {
            return currentSagD;
        }
        inline float GetSagValue() 
        {
            return currentSag;
        }
        inline float TickOutput(float value)
        {
            float powerInput = value*currentSagD;

            currentPower = std::fabs(powerFilter.Tick(powerInput*powerInput));
            currentSag = 1.0f/(currentPower*(sagAf-1)+1);
            currentSagD = 1.0f/(currentPower*(sagDAf-1)+1);

            return value;


        }

    }
;}