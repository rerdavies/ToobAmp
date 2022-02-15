#pragma once

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



namespace TwoPlay {

class ControlDezipper {
private:
    float sampleRate = 44100;
    float x = 0;
    float targetX = 0;
    float dx = 0;
    size_t samplesRemaining = 0;;
    
public:
    ControlDezipper(float initialValue = 0)
    {
        To(initialValue,0);
    }
    void SetSampleRate(double sampleRate)
    {
        this->sampleRate = (float)sampleRate;
    }

    bool IsComplete() { return samplesRemaining == 0; }
    void To(float value, float timeInSeconds)
    {
        if (value == x)
        {
            samplesRemaining = 0;
            dx = 0;
            x = targetX = value;

        } else {
            samplesRemaining = (size_t)(timeInSeconds*sampleRate);
            if (samplesRemaining == 0)
            {
                x = targetX = value;
                dx = 0;
            } else {
                targetX = value;
                dx = (targetX-x)/samplesRemaining;
            }
        }
    }

    float Tick()
    {
        if (samplesRemaining != 0)
        {
            x + dx;
            if (--samplesRemaining == 0)
            {
                x = targetX;
            }
        }
        return x;
    }
};

}// namespace
