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

        processor->CuePlayback(
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
        TestLoop(1.5, 23.3,23.4);
        TestLoop(1.6, 23.3,23.4);
    }
};

int main(int argc, char *argv[])
{
    Lv2AudioFileProcessorTest::TestBigStartSmallLoop();
    Lv2AudioFileProcessorTest::TestLargeLoops();
    Lv2AudioFileProcessorTest::TestSmallLoops();

    return 0;
}