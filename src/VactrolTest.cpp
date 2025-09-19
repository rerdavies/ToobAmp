#include "LsNumerics/Vactrol.hpp"
#include "VactrolDsp.hpp"
#include "VactrolDsp.hpp"
#include <cassert>
#include <iostream>
#include <fstream>


using namespace std;
using namespace LsNumerics;

class CosOsc {
public:
    CosOsc(double sampleRate) 
    :sampleRate(sampleRate) {

    }
    void SetFrequency(float frequency) {
        this->frequency = frequency;
        phaseIncrement = 2.0 * M_PI * frequency / sampleRate;
    }

    double Tick() {
        double output = std::cos(phase);
        phase += phaseIncrement;
        if (phase >= 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
        return output;
    }

private:
    double frequency = 440.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
private:
    double sampleRate;
};

void TestVactrolSin()
{
    constexpr double SR = 48000;
    constexpr double RATE = 10;
    constexpr double DEPTH = 1.0;

    Vactrol vactrol;
    vactrol.SetRate(SR);


    CosOsc cosOsc(SR);
    cosOsc.SetFrequency(RATE);
    tremolo::Dsp tremeloDsp;
    tremeloDsp.init((unsigned int)SR);
    tremeloDsp.wetdry(100.0);
    tremeloDsp.rate(RATE);
    tremeloDsp.depth(DEPTH);
    tremeloDsp.waveform(1.0);
    
    

    std::ofstream f{"/tmp/tmp.txt"};

    for (size_t i = 0; i < 44100*2; ++i)
    {
        double cos = cosOsc.Tick();

        double myResult = vactrol.tick(0.5*(1+cos));
        double theirResult = tremeloDsp.tick(1.0f);

        double error = std::abs(myResult-theirResult);
        if (error > 1E-3) {
            throw std::runtime_error("Invalid result");
        }

        if (i % 100 == 0)
        {

            f << (i / (double)SR) << " " << myResult << " " << theirResult << endl;
        }
 
        // double error = std::abs(myResult-theirResult);
    }


}


int main(void)
{
    TestVactrolSin();
}