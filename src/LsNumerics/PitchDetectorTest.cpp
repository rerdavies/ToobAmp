
#include "PitchDetector.hpp"
#include "LsMath.hpp"
#include <cmath>
#include <random>
#include <limits>
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

using namespace LsNumerics;

std::random_device randEngine;
std::uniform_real_distribution<float> randDist(-1.0f, 1.0f);

static std::filesystem::path GetTestOutputFile()
{
    std::filesystem::path testDirectory = std::filesystem::path(getenv("HOME")) / "testOutput";
    std::filesystem::create_directories(testDirectory);
    return testDirectory / "pitchTest.tsv";
}
static void testPitchDetection()
{
    std::vector<double> sampleRates{{24000, 22050}};

    std::vector<float> buffer;

    std::vector<std::pair<double,double> > errors;
    for (auto sampleRate : sampleRates)
    {
        errors.resize(0);
        PitchDetector pitchDetector(sampleRate);
        std::cout << "Fs: " << sampleRate << " fftSize: " << pitchDetector.getFftSize() << std::endl;

        buffer.resize(pitchDetector.getFftSize());

        double minError = std::numeric_limits<double>::max();
        double maxError = -std::numeric_limits<double>::max();


        for (double f = 80; f < 1200; f += 2)
        {

            double phase = randDist(randEngine)*Pi;
            double expectedResult = FrequencyToMidiNote(f);
            for (size_t i = 0; i < buffer.size(); ++i)
            {
                buffer[i] = 
                    (float)std::sin(2 * Pi * f * i / sampleRate + phase)
                    + 0.7*(float)std::sin(4 * Pi * f * i / sampleRate + phase)
                    + 0.3*(float)std::sin(6 * Pi * f * i / sampleRate + phase)
                    ;
            }
            double fResult = pitchDetector.detectPitch(&buffer[0]);

            double expectedBinNumber = sampleRate/f;
            double binNumber = sampleRate/fResult;


            double result = FrequencyToMidiNote(fResult);
            double error = (result - expectedResult);

            //errors.push_back({f,(binNumber-expectedBinNumber)*expectedBinNumber});
            // errors.push_back({f,fResult-f});
            errors.push_back({f,error});
            if (abs(error) > 0.001)
            {
                // std::cout << "f: " << f << " error: " << error << std::endl;
            }
            if (abs(error) > 1)
            {
                std::cout << "f: " << f << " error: " << error << std::endl;
            }

            if (error > maxError)
            {
                maxError = error;
            }
            if (error < minError)
            {
                minError = error;
            }
        }
#if 1
        {
            std::ofstream f;
            f.open(GetTestOutputFile());
            assert(!f.fail());
            for (int i = 0; i < errors.size(); ++i)
            {
                const auto &t = errors[i];
                f << t.first << "\t" << t.second << '\n';
            }
            f << '\n';
        }
#endif


        std::cout << "Max error:" << maxError << " Min error: " << minError << std::endl
                  << std::endl;
    }
}


static void fftCheck()
{
    int FFT_SIZE = 4096;

    Fft<double> fft{FFT_SIZE};
    std::vector<std::complex<double>> input;
    std::vector<std::complex<double>> scratch;
    std::vector<std::complex<double>> output;

    std::default_random_engine r;

    input.resize(FFT_SIZE);
    scratch.resize(FFT_SIZE);
    output.resize(FFT_SIZE);

    for (int i = 0; i < FFT_SIZE; ++i)
    {
        input[i] = randDist(randEngine);
    }
    fft.forward(input, scratch);
    fft.backward(scratch, output);

    for (size_t i = 0; i < FFT_SIZE; ++i)
    {
        double error = std::abs(input[i] - output[i]);
        assert(error < 1E-7);
    }
}

int main(int, char **)
{
    fftCheck();
    testPitchDetection();

    return 0;
}