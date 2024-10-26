
#include "PitchDetector.hpp"
#include "IfPitchDetector.hpp"
#include "LsMath.hpp"
#include <cmath>
#include <random>
#include <limits>
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include "Window.hpp"
#include "../TestAssert.hpp"

using namespace LsNumerics;

#define UNUSED_VARIABLE(x) ((void)x)
std::mt19937 randEngine;
std::uniform_real_distribution<float> randDist(-1.0f, 1.0f);

static std::filesystem::path GetTestOutputFile()
{
    std::filesystem::path testDirectory = std::filesystem::path(getenv("HOME")) / "testOutput";
    std::filesystem::create_directories(testDirectory);
    return testDirectory / "pitchTest.tsv";
}

double noiseLevel = Db2Af(-35);

double F(size_t t, double f, double sampleRate)
{
    double result = (float)std::sin(2 * Pi * f * t / sampleRate) + 0.1 * (float)std::sin(4 * Pi * f * (t + 1) / sampleRate) + 0.3 * (float)std::sin(6 * Pi * f * (t + 2) / sampleRate);

    //double noise = randDist(randEngine);
    //result += noise * noiseLevel;
    return result;
}
static void testPitchDetection()
{
    // constexpr size_t SAMPLE_RATE = 24000;
    // constexpr size_t FFT_SIZE = 4096;
    // constexpr size_t SAMPLE_STRIDE = 2048;

    std::vector<double> sampleRates{{24000, 22050}};

    std::vector<float> buffer;

    std::vector<std::vector<double>> errors;
    for (auto sampleRate : sampleRates)
    {
        errors.resize(0);
        PitchDetector pitchDetector(sampleRate);
        std::cout << "Fs: " << sampleRate << " fftSize: " << pitchDetector.getFftSize() << std::endl;
        // pitchDetector.Window() = Window::Hann<double>(pitchDetector.getFftSize());

        buffer.resize(pitchDetector.getFftSize() * 2);

        double minError = std::numeric_limits<double>::max();
        double maxError = -std::numeric_limits<double>::max();

        double maxErrorFrequency = 0, minErrorFrequency = 0;

        for (double f = 40; f < 923; f += 2)
        {

            double phase = randDist(randEngine) * Pi;
            UNUSED_VARIABLE(phase);
            double frequencyMinError = 1E100;
            double frequencyMaxError = -1E100;

            double expectedResult = FrequencyToMidiNote(f);
            size_t ix = 100;
            for (size_t frame = 0; frame < 40; ++frame)
            {
                for (size_t i = 0; i < buffer.size(); ++i)
                {
                    buffer[i] =

                        F(ix++, f, sampleRate);
                }
                double fResult = pitchDetector.detectPitch(&buffer[0]);

                double expectedBinNumber = sampleRate / f;
                UNUSED_VARIABLE(expectedBinNumber);
                double binNumber = sampleRate / fResult;
                UNUSED_VARIABLE(binNumber);

                double result = FrequencyToMidiNote(fResult);
                double error = (result - expectedResult);

                if (error < frequencyMinError)
                    frequencyMinError = error;
                if (error > frequencyMaxError)
                    frequencyMaxError = error;
                // errors.push_back({f,(binNumber-expectedBinNumber)*expectedBinNumber});
                // errors.push_back({f,fResult-f});

                if (f >= 82)
                {
                    if (error > maxError)
                    {
                        maxError = error;
                        maxErrorFrequency = f;
                    }
                    if (error < minError)
                    {
                        minError = error;
                        minErrorFrequency = f;
                    }
                }
            }
            using namespace std;
            cout
                << fixed << setw(5) << setprecision(0) << f
                << ", " << setw(8) << fixed << setprecision(4) << frequencyMinError
                << ", " << setw(8) << fixed << setprecision(4) << frequencyMaxError
                << endl;
        }
#if 1
        {
            std::ofstream f;
            f.open(GetTestOutputFile());
            assert(!f.fail());
            for (size_t i = 0; i < errors.size(); ++i)
            {
                const auto &t = errors[i];

                bool isFirst = true;

                for (double v : t)
                {
                    if (!isFirst)
                        f << ",";
                    isFirst = false;
                    f << v;
                }
                f << endl;
            }
            f << '\n';
        }
#endif

        std::cout << "Max error:" << maxError * 100 << " cents (" << maxErrorFrequency << " Hz)" << std::endl
                  << "Min error: " << minError * 100 << " cents (" << minErrorFrequency << " Hz)" << std::endl;
    }
}

void testIfPitchDetection()
{
    constexpr size_t SAMPLE_RATE = 24000;
    constexpr size_t FFT_SIZE = 4096;
    constexpr size_t SAMPLE_OFFSET = FFT_SIZE / 2;

    std::vector<double> sampleRates{SAMPLE_RATE};

    std::vector<float> buffer;

    std::vector<std::pair<double, double>> errors;
    for (auto sampleRate : sampleRates)
    {
        errors.resize(0);
        IfPitchDetector pitchDetector(sampleRate, FFT_SIZE);
        std::cout << "Fs: " << sampleRate << " fftSize: " << pitchDetector.getFftSize() << std::endl;
        // pitchDetector.Window() = Window::Hann<double>(FFT_SIZE);

        buffer.resize(pitchDetector.getFftSize() * 2);

        double minError = std::numeric_limits<double>::max();
        double maxError = -std::numeric_limits<double>::max();

        for (double f = 80; f < 923; f += 2)
        {

            double phase = randDist(randEngine) * Pi;
            UNUSED_VARIABLE(phase);
            double expectedResult = FrequencyToMidiNote(f);
            for (size_t i = 0; i < buffer.size(); ++i)
            {
                buffer[i] =

                    F(i + 100, f, sampleRate);
            }
            pitchDetector.prime(buffer, 0);
            double fResult = pitchDetector.detectPitch(buffer, SAMPLE_OFFSET, SAMPLE_OFFSET);

            double expectedBinNumber = sampleRate / f;
            UNUSED_VARIABLE(expectedBinNumber);
            double binNumber = sampleRate / fResult;
            UNUSED_VARIABLE(binNumber);

            double result = FrequencyToMidiNote(fResult);
            double error = (result - expectedResult);

            // errors.push_back({f,(binNumber-expectedBinNumber)*expectedBinNumber});
            // errors.push_back({f,fResult-f});
            errors.push_back({f, error});
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
            TEST_ASSERT(!f.fail());
            for (size_t i = 0; i < errors.size(); ++i)
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
    size_t FFT_SIZE = 4096;

    Fft fft{FFT_SIZE};
    std::vector<std::complex<double>> input;
    std::vector<std::complex<double>> scratch;
    std::vector<std::complex<double>> output;

    std::default_random_engine r;

    input.resize(FFT_SIZE);
    scratch.resize(FFT_SIZE);
    output.resize(FFT_SIZE);

    for (size_t i = 0; i < FFT_SIZE; ++i)
    {
        input[i] = randDist(randEngine);
    }
    fft.Forward(input, scratch);
    fft.Backward(scratch, output);

    for (size_t i = 0; i < FFT_SIZE; ++i)
    {
        double error = std::abs(input[i] - output[i]);
        TEST_ASSERT(error < 1E-7);
    }
}

int main(int, char **)
{
    fftCheck();
    testPitchDetection();

    return 0;
}