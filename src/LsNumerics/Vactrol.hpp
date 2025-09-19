#pragma once 

// Copyright (c) Robin E. R. Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

/*
** Based Model of a vactrol tremolo unit by "transmogrify"
** c.f. http://sourceforge.net/apps/phpbb/guitarix/viewtopic.php?f=7&t=44&p=233&hilit=transmogrifox#p233
** http://transmogrifox.webs.com/vactrol.m
*/
#include <cmath>

namespace LsNumerics {

    class Vactrol {
        static constexpr double R1 = 2700;
        static constexpr double Ra = 1e6;
        static constexpr double Rb = 300;
        static constexpr double dTC = 0.06;
        double Exp1 = exp(1);
        double LogRb = log(Rb);
        double LogRa = log(Ra);

        double b = 0;
        double minTC = 0;
        double y0 = 0;
        double iSR = 0;
        double cds_y1 = 0;
    public:
        void SetRate(double rate) { 
            this->rate = rate; 

            this->b = exp(LogRa/LogRb)-Exp1;
            this->minTC = log(0.005/dTC);

            this->iSR = 1/rate;
        }
    
        double dRC(double value) {
            return dTC*exp(value*(minTC));
        }
        double alpha(double value)
        {
            return 1-iSR/(dRC(value)+iSR);
        }

        float vectrol_l2(float value)
        {
            return value*b+Exp1;
        }
        float vectrol_l3(float value) {
            return exp(LogRa/log(value));
        }
        float vectrol_l4(float value)
        {
            return R1/(value+R1);
        }
        float cds(float value) {
            double cds_y0 = cds_y1*alpha(cds_y1);
            double y0 = cds_y0 + (1-alpha(cds_y0))*value;
            cds_y1 = y0;
            return y0;
        }
        float tick(float value) {
            return vectrol_l4(vectrol_l3(vectrol_l2(cds(pow(value,1.9)))));
        }


    private:
        double rate;
    };
};