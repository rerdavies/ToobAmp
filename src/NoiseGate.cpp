#include "std.h"
#include "NoiseGate.h"
#include "ToobMath.h"



using namespace TwoPlay;

static const double ATTACK_SECONDS = 0.001;
static const double RELEASE_SECONDS = 0.3;
static const double HOLD_SECONDS = 0.2;


int32_t NoiseGate::SecondsToSamples(double seconds)
{
    return (int32_t)sampleRate*seconds;
}

void NoiseGate::SetGateThreshold(float decibels)
{
     this->afAttackThreshold = TwoPlay::Db2Af(decibels);
    this->afReleaseThreshold = this->afAttackThreshold*0.25f;
}
double NoiseGate::SecondsToRate(double seconds)
{
    return 1/(seconds*sampleRate);
}

void NoiseGate::SetSampleRate(double sampleRate)
{
    this->sampleRate = sampleRate;
    this->attackRate = SecondsToRate(ATTACK_SECONDS);
    this->releaseRate = SecondsToRate(RELEASE_SECONDS);
    this->holdSampleDelay = SecondsToSamples(HOLD_SECONDS);
}