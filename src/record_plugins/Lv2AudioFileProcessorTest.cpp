/*
 *   Copyright (c) 2025 Robin E. R. Davies
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

#include "Lv2AudioFileProcessor.hpp"
#include <cstring>
#include <iostream>

#include "FfmpegDecoderStream.hpp"

#include "../LsNumerics/LsMath.hpp"

using namespace toob;
using namespace pipedal;
using namespace std;

class Lv2AudioFileProcessorTest
{
public:
    static std::unique_ptr<Lv2AudioFileProcessor> CreateProcessor(
        double sampleRate = 44100.0,
        int channels = 2)
    {
        auto p = std::make_unique<Lv2AudioFileProcessor>(nullptr, sampleRate, channels);
        p->Activate();
        return p;
    }

    static void WaitForReady(Lv2AudioFileProcessor *processor)
    {
        for (int retry = 0; retry < 100; ++retry)
        {
            processor->HandleMessages();
            if (processor->GetState() == ProcessorState::Playing)
            {
                if (processor->fgPlaybackQueue.size() > 5 || processor->fgLoopBuffer.Get() != nullptr)
                {
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        throw std::runtime_error("Timed out waiting for processor to be ready.");
    }

    static void TestFileLoop(
        double dStart,
        double dLoopStart,
        double dLoopEnd,
        double sampleRate = 44100.0)
    {
        std::filesystem::path testFilePath = "Assets/LoopTest/chirp.mp3";

        auto processor = CreateProcessor();

        LoopParameters loopParams{
            dStart,
            true,
            dLoopStart,
            dLoopEnd};
        double duration = dLoopEnd + (GetLoopBlendLength(sampleRate) + 5) / sampleRate;
        LoopControlInfo controlInfo(loopParams, sampleRate, duration);

        size_t length = size_t(std::round(duration * sampleRate));

        std::vector<float> testDataL(length);
        std::vector<float> testDataR(length);

        {
            FfmpegDecoderStream decoderStream{};
            decoderStream.open(
                testFilePath,
                2, // stereo
                sampleRate,
                0);
            size_t testSize = (size_t)(length + sampleRate * 0.2);
            testDataL.resize(testSize);
            testDataR.resize(testSize);

            float *buffers[2];
            buffers[0] = testDataL.data();
            buffers[1] = testDataR.data();
            size_t nRead = decoderStream.read(buffers, length);
            if (nRead < length)
            {
                std::cerr << "Warning: Only read " << nRead << " samples from test file, expected " << length << "." << std::endl;
            }
            if (nRead < testSize)
            {
                for (size_t i = nRead; i < testSize; ++i)
                {
                    testDataL[i] = 0.0f;
                    testDataR[i] = 0.0f;
                }
            }
        }

        std::stringstream ss;
        json_writer writer(ss);
        ToobPlayerSettings settings;
        settings.loopParameters_ = loopParams;
        writer.write(&settings);
        std::string loopParamsStr = ss.str();

        processor->TestCuePlayback(
            testFilePath.string().c_str(),
            loopParamsStr.c_str());


        WaitForReady(processor.get());

        size_t testLength = controlInfo.start + controlInfo.loopSize * 3;

        size_t position = controlInfo.start;
        ;

        std::vector<float> bufferL(1024);
        std::vector<float> bufferR(1024);

        while (position < testLength)
        {
            for (size_t i = 0; i < bufferL.size(); ++i)
            {
                bufferL[i] = 0.0f;
            }
            for (size_t i = 0; i < bufferR.size(); ++i)
            {
                bufferR[i] = 0.0f;
            }

            WaitForReady(processor.get());

            processor->Play(bufferL.data(), bufferR.data(), bufferL.size());

            for (size_t i = 0; i < bufferL.size(); ++i)
            {
                float left = bufferL[i];
                float right = bufferR[i];
                if (left != right)
                {
                    throw std::logic_error("Left and right channels do not match.");
                }
                float expectedValue;
                if (position < controlInfo.loopStart)
                {
                    expectedValue = testDataL[position];
                }
                else if (position >= controlInfo.loopStart && position < controlInfo.loopEnd_0)
                {
                    expectedValue = testDataL[position];
                }
                else
                {
                    size_t loopPosition = position;
                    while (loopPosition >= controlInfo.loopEnd_1)
                    {
                        loopPosition -= controlInfo.loopSize;
                    }
                    if (loopPosition < controlInfo.loopEnd_0)
                    {
                        expectedValue = testDataL[loopPosition];
                    }
                    else
                    {
                        size_t ix0 = loopPosition - controlInfo.loopSize;
                        if (ix0 >= testDataL.size())
                        {
                            std::cerr << "Position out of bounds: " << position << std::endl;
                            throw std::logic_error("Position out of bounds in test data.");
                        }
                        float v1 = testDataL[ix0];
                        float v0 = testDataL[loopPosition];
                        float blendFactor = (float)(loopPosition - controlInfo.loopEnd_0) / (float)(controlInfo.loopEnd_1 - controlInfo.loopEnd_0);
                        expectedValue = v0 * (1.0f - blendFactor) + v1 * blendFactor;
                    }
                }
                if (std::abs(bufferL[i] - expectedValue) > 0.001f ||
                    std::abs(bufferR[i] - expectedValue) > 0.001f)
                {
                    std::cerr << "Mismatch at position " << position << ": "
                              << "L: " << bufferL[i] << ", R: " << bufferR[i]
                              << " expected L: " << expectedValue
                              << ", R: " << expectedValue << std::endl;
                    throw std::logic_error("Loop data mismatch.");
                }
                ++position;
            }
        }
    }

    static void TestSeek(
        double dStart,
        double sampleRate = 48000.0)
    {
        size_t target = size_t(std::round(dStart * sampleRate));
        dStart = target/sampleRate; 
        cout << "TestSeek " << dStart << "s at " << sampleRate << " Hz." << endl;

        std::filesystem::path testFilePath = "Assets/LoopTest/chirp.mp3";

        std::vector<float> testDataL;
        std::vector<float> testDataR;

        size_t maxSample;
        double maxDuration;

        {

            FfmpegDecoderStream decoderStream{};
            decoderStream.open(
                testFilePath,
                2, // stereo
                sampleRate,
                0);
            size_t testSize = (size_t)(80 * sampleRate);
            testDataL.resize(testSize);
            testDataR.resize(testSize);

            float *buffers[2];
            buffers[0] = testDataL.data();
            buffers[1] = testDataR.data();
            size_t nRead = decoderStream.read(buffers, testSize);
            if (nRead == 0)
            {
                throw std::runtime_error("Failed to read any data from the test file.");
            }
            testDataL.resize(nRead);
            testDataR.resize(nRead);

            maxSample = nRead;
            maxDuration = nRead / sampleRate;
        }
        cout << "Duration: " << maxDuration << " seconds, Samples: " << maxSample << endl;

        double dMin = std::numeric_limits<double>::max();
        double dMax = std::numeric_limits<double>::lowest();
        int64_t  eMin = std::numeric_limits<int64_t>::max();
        int64_t eMax = std::numeric_limits<int64_t>::lowest();
        size_t sampleOffset = size_t(std::round(dStart * sampleRate));

        for (double delta = -3.0; delta <= 3.0; delta += 1 / 4.0)
        {
            size_t desiredIndex = size_t(std::round(dStart * sampleRate));
            double thisSeekPos = (desiredIndex + delta) / sampleRate;
            if (thisSeekPos < 0.001)
            {
                continue;
            }
            FfmpegDecoderStream decoderStream{};
            decoderStream.open(
                testFilePath,
                2, // stereo
                sampleRate,
                thisSeekPos);

            std::vector<float> bufferL(10);
            std::vector<float> bufferR(10);

            float *buffers[2];
            buffers[0] = bufferL.data();
            buffers[1] = bufferR.data();
            size_t nRead = decoderStream.read(buffers, bufferL.size());
            if (nRead != bufferL.size())
            {
                throw std::logic_error("Read did not return expected number of samples.");
            }

            int64_t dI = -100;
            for (int64_t i = -50; i <= 50; ++i)
            {
                if (sampleOffset + i < 0)
                {
                    continue;
                }
                if (testDataL[sampleOffset + i] == bufferL[0])
                {
                    dI = i;
                }
            }
            cout <<  "   dI: " << dI << " delta: " << delta << endl;

            eMin = std::min(eMin, dI);
            eMax = std::max(eMax, dI);

            if (testDataL[sampleOffset] == bufferL[0])
            {
                dMin = std::min(dMin, thisSeekPos);
                dMax = std::max(dMax, thisSeekPos);
            }
        }
        if (dMin <= dMax)
        {
            cout << "Good seek positions: " << dMin << " to " << dMax << endl;
            cout << "   sampleOffsets:  " << ((dMin * sampleRate)-sampleOffset) << " to " << ((dMax * sampleRate)-sampleOffset) << endl;
            cout << "   sampleOffset % 4: " << (sampleOffset % 4) << endl;
        }
        else
        {
            cout << "   eMin: " << eMin << ", eMax: " << eMax << endl;
            cout << "   Not found." << endl;
            cout << "   sampleOffset % 4: " << (sampleOffset % 4) << endl;
        }
    }

    static void TestLoop(
        double dStart,
        double dLoopStart,
        double dLoopEnd,
        double sampleRate = 44100.0)
    {
        auto processor = CreateProcessor();

        LoopParameters loopParams{
            dStart,
            true,
            dLoopStart,
            dLoopEnd};
        double duration = dLoopEnd + (GetLoopBlendLength(sampleRate) + 5) / sampleRate;
        LoopControlInfo controlInfo(loopParams, sampleRate, duration);

        size_t length = size_t(std::round(duration * sampleRate));

        std::vector<float> testDataL(length);
        std::vector<float> testDataR(length);

        for (size_t i = 0; i < controlInfo.start; ++i)
        {
            testDataL[i] = 0.01f;
            testDataR[i] = 0.01f;
        }
        for (size_t i = controlInfo.start; i < controlInfo.loopStart; ++i)
        {
            testDataL[i] = 0.1f;
            testDataR[i] = 0.1f;
        }

        for (size_t i = controlInfo.loopStart; i < controlInfo.loopEnd; ++i)
        {
            testDataL[i] = -1.0f;
            testDataR[i] = -1.0f;
        }
        for (size_t i = controlInfo.loopEnd; i < length; ++i)
        {
            testDataL[i] = 100.0f;
            testDataR[i] = 100.0f;
        }
        testDataL[controlInfo.start] = 0.9f;
        testDataR[controlInfo.start] = 0.9f;
        testDataL[controlInfo.loopStart] = 9.0f;
        testDataR[controlInfo.loopStart] = 9.0f;

        processor->bgReader.Test_SetFileData(testDataL, testDataR);

        std::stringstream ss;
        json_writer writer(ss);
        ToobPlayerSettings settings;
        settings.loopParameters_ = loopParams;
        writer.write(&settings);
        std::string loopParamsStr = ss.str();

        processor->TestCuePlayback(
            "dummy.wav",
            loopParamsStr.c_str(),
            controlInfo.start,
            false);

        WaitForReady(processor.get());

        size_t testLength = controlInfo.start + controlInfo.loopSize * 3;

        size_t position = controlInfo.start;
        ;

        std::vector<float> bufferL(1024);
        std::vector<float> bufferR(1024);

        while (position < testLength)
        {
            for (size_t i = 0; i < bufferL.size(); ++i)
            {
                bufferL[i] = 0.0f;
            }
            for (size_t i = 0; i < bufferR.size(); ++i)
            {
                bufferR[i] = 0.0f;
            }

            WaitForReady(processor.get());

            processor->Play(bufferL.data(), bufferR.data(), bufferL.size());

            for (size_t i = 0; i < bufferL.size(); ++i)
            {
                float left = bufferL[i];
                float right = bufferR[i];
                if (left != right)
                {
                    throw std::logic_error("Left and right channels do not match.");
                }
                float expectedValue;
                if (position < controlInfo.loopStart)
                {
                    expectedValue = testDataL[position];
                }
                else if (position >= controlInfo.loopStart && position < controlInfo.loopEnd_0)
                {
                    expectedValue = testDataL[position];
                }
                else
                {
                    size_t loopPosition = position;
                    while (loopPosition >= controlInfo.loopEnd_1)
                    {
                        loopPosition -= controlInfo.loopSize;
                    }
                    if (loopPosition < controlInfo.loopEnd_0)
                    {
                        expectedValue = testDataL[loopPosition];
                    }
                    else
                    {
                        size_t ix0 = loopPosition - controlInfo.loopSize;
                        if (ix0 >= testDataL.size())
                        {
                            std::cerr << "Position out of bounds: " << position << std::endl;
                            throw std::logic_error("Position out of bounds in test data.");
                        }
                        float v1 = testDataL[ix0];
                        float v0 = testDataL[loopPosition];
                        float blendFactor = (float)(loopPosition - controlInfo.loopEnd_0) / (float)(controlInfo.loopEnd_1 - controlInfo.loopEnd_0);
                        expectedValue = v0 * (1.0f - blendFactor) + v1 * blendFactor;
                    }
                }
                if (std::abs(bufferL[i] - expectedValue) > 0.001f ||
                    std::abs(bufferR[i] - expectedValue) > 0.001f)
                {
                    std::cerr << "Mismatch at position " << position << ": "
                              << "L: " << bufferL[i] << ", R: " << bufferR[i]
                              << " expected L: " << expectedValue
                              << ", R: " << expectedValue << std::endl;
                    throw std::logic_error("Loop data mismatch.");
                }
                ++position;
            }
        }
    }

    static void TestLargeLoops()
    {
        std::cout << "Testing large loops..." << std::endl;
        TestLoop(1.0, 5.5, 20.5);
        TestLoop(1.0, 5.0, 20.0);
        TestLoop(0.0, 5.0, 20.0);
        TestLoop(6.5, 1.5, 19.5);
        TestLoop(19.42, 1.5, 19.5);
    }
    static void TestSmallLoops()
    {
        std::cout << "Testing small loops..." << std::endl;
        TestLoop(22.04, 19.3, 22.5);
        TestLoop(5.5, 5.025, 6.026);
        TestLoop(0.0, 0.5, 0.8);
        TestLoop(0.0, 0.025, 0.035);
        TestLoop(0.5, 5.025, 6.026);
    }
    static void TestBigStartSmallLoop()
    {
        std::cout << "Testing big-start/small-loops..." << std::endl;
        TestLoop(1.0, 19.0, 20.0);
        TestLoop(0.0, 19.3, 22.5);
        TestLoop(1.5, 23.3, 23.4);
        TestLoop(1.6, 23.3, 23.4);
    }
    static void TestFileLoops()
    {
        std::cout << "Testing big-start/small-loops..." << std::endl;
        TestFileLoop(19.1433532, 18.13483, 20.3493958); // small loop.
        TestFileLoop(0, 0, 5.32415);                    // small loop.
        TestFileLoop(1.23433, 5.134323, 20.193473);     // big loop
        TestFileLoop(10.23433, 15.134323, 30.193473);     // big loop
        TestFileLoop(0.0, 19.313134, 22.56663);         // big/samll
    }
    static void AnalyzeSeeks()
    {
        cout << "Analyzing seeks..." << std::endl;
        // TestSeek(0.0);
        TestSeek(20.123435);
        TestSeek(40.31531);
        TestSeek(55.5235);
        TestSeek(55.6429);
    }
};

int main(int argc, char *argv[])
{
    // Lv2AudioFileProcessorTest::AnalyzeSeeks();
    Lv2AudioFileProcessorTest::TestFileLoops();
    Lv2AudioFileProcessorTest::TestBigStartSmallLoop();
    Lv2AudioFileProcessorTest::TestLargeLoops();
    Lv2AudioFileProcessorTest::TestSmallLoops();

    return 0;
}