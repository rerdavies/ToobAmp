
#include "PitchDetector.hpp"
#include "IfPitchDetector.hpp"
#include "LsMath.hpp"
#include <cmath>
#include <random>
#include <limits>
#include <iostream>
#include <cassert>
#include <sstream>
#include <fstream>
#include <filesystem>
#include "Window.hpp"
#include "../TestAssert.hpp"
#include "../FlacReader.hpp"
#include <filesystem>
#include "../CommandLineParser.hpp"
#include <string>

using namespace LsNumerics;
namespace fs = std::filesystem;

#pragma GCC diagnostic ignored "-Wunused-variable" // GCC 12 bug.

#define UNUSED_VARIABLE(x) ((void)x)
std::mt19937 randEngine;
std::uniform_real_distribution<float> randDist(-1.0f, 1.0f);

constexpr int MIDI_A440 = 69;
// Function to convert frequency to MIDI note number (exact)
double frequencyToMidiNote(double freq)
{
    if (freq <= 0)
        return -1.0; // Invalid frequency
    return MIDI_A440 + 12 * std::log2(freq / 440.0);
}

double midiNoteToFrequency(int midiNote)
{
    return pow(2, (midiNote - MIDI_A440) / 12.0) * 440.0;
}

// Function to convert MIDI note number to note name
std::string midiNoteToName(int midiNote)
{
    if (midiNote < 0 || midiNote > 127)
        return "Invalid";

    const std::string noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = midiNote / 12 - 1;
    int noteIndex = midiNote % 12;
    return noteNames[noteIndex] + std::to_string(octave);
}

// Function to convert frequency to note name with cents
std::string frequencyToNoteName(double freq)
{
    double midiNoteExact = frequencyToMidiNote(freq);
    if (midiNoteExact < 0)
        return "Invalid";

    int midiNote = std::round(midiNoteExact);
    double cents = 100 * (midiNoteExact - midiNote); // Cents deviation

    std::ostringstream oss;
    oss << midiNoteToName(midiNote);
    // Only append cents if non-zero (avoid unnecessary +0.00)
    if (std::abs(cents) > 0.01)
    { // Threshold for floating-point noise
        oss << (cents >= 0 ? "+" : "") << std::fixed << std::setprecision(2) << cents;
    }
    return oss.str();
}

static std::filesystem::path GetTestOutputFile()
{
    std::filesystem::path testDirectory = std::filesystem::path(getenv("HOME")) / "testOutput";
    std::filesystem::create_directories(testDirectory);
    return testDirectory / "pitchTest.tsv";
}

double noiseLevel = Db2Af(-35);

double F(size_t t, double f, double sampleRate)
{
    double result = (float)std::sin(
        2 * Pi * f * t / sampleRate) 
        + 0.1 * (float)std::sin(4 * Pi * f * (t + 1) / sampleRate) + 0.3 * (float)std::sin(6 * Pi * f * (t + 2) / sampleRate);

    // double noise = randDist(randEngine);
    // result += noise * noiseLevel;
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
            for (size_t frame = 0; frame < 40; ++frame)
            {
                double iPhase = frame/40.0;

                for (size_t i = 0; i < buffer.size(); ++i)
                {
                    buffer[i] =
                        sin(
                            2* Pi*(iPhase + i*f/sampleRate)
                        ) 
                        // + 0.5*sin(
                        //     4* Pi*(iPhase + 2*i*f/sampleRate)
                        // )
                        + 0.03*sin(
                            6* Pi*(iPhase + 2*i*f/sampleRate)
                        )
                        ;
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

static void generateBiasTables()
{
    std::vector<double> sampleRates{{24000, 22050}};

    std::vector<float> buffer;

    std::vector<std::vector<double>> errors;

    cout << "// Generated by PitchDetectorTest --bias_tables" << endl;
    cout << "// Windowing causes pitches to be slightly underestimated. These tables are " << endl;
    cout << "// used to center the error range around the correct pitch in order to reduce " << endl;
    cout << "// the error by a factor of two. " << endl;
    cout << endl;

    for (auto sampleRate : sampleRates)
    {
        cout << "static float fm" << sampleRate << "_bias_table[] = " << endl;
        cout << "{" << endl;

        PitchDetector pitchDetector(sampleRate);

        errors.resize(0);

        buffer.resize(pitchDetector.getFftSize() * 2);

        for (int midiNote = 0; midiNote < 128; ++midiNote)
        {
            double f = midiNoteToFrequency(midiNote);
            if (f <= PitchDetector::MINIMUM_DETECTABLE_FREQUENCY || f >= PitchDetector::MAXIMUM_DETECTABLE_FREQUENCY)
            {
                cout << "    0," << endl;
            }
            else
            {

                double phase = randDist(randEngine) * Pi;
                UNUSED_VARIABLE(phase);
                double frequencyMinError = 1E100;
                double frequencyMaxError = -1E100;

                double expectedResult = midiNote;
                for (size_t frame = 0; frame < 40; ++frame)
                {
                    double iPhase = frame/40.0;
                    for (size_t i = 0; i < buffer.size(); ++i)
                    {
                        buffer[i] =
                            sin(
                                2* Pi*(iPhase + i*f/sampleRate)
                            )
                            // + 
                            // 0.3*sin(
                            //     6* Pi*(iPhase + i*f/sampleRate)
                            // )
                        ;
                    }
                    frequencyMinError = 0;
                    frequencyMaxError = 0;
                    double fResult = pitchDetector.detectPitch(&buffer[0]);
                    if (fResult != 0)
                    {
                        double result = frequencyToMidiNote(fResult);

                        double error = (result - expectedResult);

                        if (error < frequencyMinError)
                            frequencyMinError = error;
                        if (error > frequencyMaxError)
                            frequencyMaxError = error;
                        // errors.push_back({f,(binNumber-expectedBinNumber)*expectedBinNumber});
                        // errors.push_back({f,fResult-f});
                    }
                }

                cout << "    " << setw(8) << (frequencyMinError + frequencyMaxError) / 2
                     << ", // " << midiNote 
                     << " " << midiNoteToFrequency(expectedResult) << "hz  min: " << frequencyMinError
                     << " max: " << frequencyMaxError << endl;
            }
        }
        cout << "};" << endl;
        cout << endl
             << endl;
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

std::vector<float> downsampleData(const std::vector<float> &data)
{
    std::vector<float> result;
    result.resize(data.size() / 2);
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = data[i * 2];
    }
    return result;
}

double rms(float *start, size_t n)
{
    double sum = 0;
    for (size_t i = 0; i < n; ++i)
    {
        float v = start[i];
        sum += v * v;
    }
    double t = sqrt(sum / n);
    return Af2Db(t);
}

void testGuitarSample(const std::filesystem::path&filename, float expectedPitch)
{
    cout << "Testing file: " << filename << endl;
    toob::AudioData data = toob::FlacReader::Load(filename);
    size_t sampleRate = data.getSampleRate() / 2;

    PitchDetector pitchDetector;
    pitchDetector.Initialize(sampleRate);
    std::vector<float> samples = downsampleData(data.getChannel(0));

    int midiNote = 12 * 4 + 11;
    const int A4 = 12 * 4 + 9;
    const double EXPECTED_FREQUENCY = 440 * pow(2.0, (midiNote - A4) / 12.0);
    const double CENTS_40 = pow(2.0, 40.0 / 12.0 / 100.0);
    const double MIN_F = EXPECTED_FREQUENCY / CENTS_40;
    const double MAX_F = EXPECTED_FREQUENCY * CENTS_40;

    int n = 0;
    for (size_t ix = 0; ix + pitchDetector.getFftSize() < samples.size(); ix += pitchDetector.getFftSize())
    {
        double f = pitchDetector.detectPitch(samples.begin() + ix);
        double dbRms = rms(&*(samples.begin() + ix), +pitchDetector.getFftSize());
        //bool valid = (f == 0) || (f >= MIN_F && f <= MAX_F);
        //if (!valid)
        {
            cout << "Error: " << n++ << " f=" << f << " rmsDb=" << dbRms << " " << frequencyToNoteName(f) << endl;
            pitchDetector.detectPitch(samples.begin() + ix);
        }
    }
}

void testGuitarSample()
{
    testGuitarSample("Assets/Guitar-E2-2.flac",40);
    testGuitarSample("Assets/Guitar-B4.flac",59.0-0.03);

}


int main(int argc, char **argv)
{

    toob::CommandLineParser cmdline;
    bool biasTables = false;

    cmdline.AddOption("", "bias-tables", &biasTables);

    try
    {
        cmdline.Parse(argc, argv);

        if (biasTables)
        {
            generateBiasTables();
        }
        else
        {
            testGuitarSample();
            fftCheck();
            testPitchDetection();
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}