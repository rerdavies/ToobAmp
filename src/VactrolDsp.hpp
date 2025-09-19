// generated from file '../src/faust/tremolo.dsp' by dsp2cc:
// Code generated with Faust (https://faust.grame.fr)
#pragma once

#include <cmath>
namespace tremolo {

    class PluginDef {

    };

#define FAUSTFLOAT float
#define always_inline

class Dsp: public PluginDef {
private:
	int fSampleRate;
	FAUSTFLOAT ctl_wetdry;
	int iVec0[2];
	double k_iSR;
	FAUSTFLOAT ctl_depth;
	FAUSTFLOAT ctl_waveform = 1.0;
	FAUSTFLOAT ctl_rate;
	double fRec1[2];
	double k_2pi_by_SR;
	double fRec4[2];
	double fRec3[2];
	double fRec2[2];
	double k_iSR_by_2;
	int iRec6[2];
	int iRec5[2];
	double f_cds_y[2];

public:
	void clear_state_f();
    void rate(float value) { ctl_rate = value; }
    void wetdry(float value) { ctl_wetdry = value; }
    void depth(float value) { ctl_depth = value; }
    void waveform(float value) { ctl_waveform = value; }
	void init(unsigned int sample_rate);
	void compute(int count, FAUSTFLOAT *input0, FAUSTFLOAT *output0);

    FAUSTFLOAT tick(FAUSTFLOAT value) 
    {
        FAUSTFLOAT out;
        compute(1,&value,&out);
        return out;
    }

public:
	Dsp();
	~Dsp();
};



Dsp::Dsp()
	: PluginDef() {
}

Dsp::~Dsp() {
}

inline void Dsp::clear_state_f()
{
	for (int l0 = 0; l0 < 2; l0 = l0 + 1) iVec0[l0] = 0;
	for (int l1 = 0; l1 < 2; l1 = l1 + 1) fRec1[l1] = 0.0;
	for (int l2 = 0; l2 < 2; l2 = l2 + 1) fRec4[l2] = 0.0;
	for (int l3 = 0; l3 < 2; l3 = l3 + 1) fRec3[l3] = 0.0;
	for (int l4 = 0; l4 < 2; l4 = l4 + 1) fRec2[l4] = 0.0;
	for (int l5 = 0; l5 < 2; l5 = l5 + 1) iRec6[l5] = 0;
	for (int l6 = 0; l6 < 2; l6 = l6 + 1) iRec5[l6] = 0;
	for (int l7 = 0; l7 < 2; l7 = l7 + 1) f_cds_y[l7] = 0.0;
}


inline void Dsp::init(unsigned int sample_rate)
{
	fSampleRate = sample_rate;
	double fConst0 = std::min<double>(1.92e+05, std::max<double>(1.0, double(fSampleRate)));
	k_iSR = 1.0 / fConst0;
	k_2pi_by_SR = 6.283185307179586 / fConst0;
	k_iSR_by_2 = 0.5 * fConst0;
	clear_state_f();
}


void always_inline Dsp::compute(int count, FAUSTFLOAT *input0, FAUSTFLOAT *output0)
{
	double f_wetdry = double(ctl_wetdry);
	double f_dry = 1.0 - 0.01 * f_wetdry;
	double f_wet_x_R1 = 27.0 * f_wetdry;
	double f_depth = double(ctl_depth);
	double f_waveform = double(ctl_waveform);
	int b_triangle = f_waveform == 0.0;
	int b_sin = f_waveform == 1.0;
	double f_rate = double(ctl_rate);
	double fSlow8 = k_iSR * f_rate;
	double fSlow9 = k_2pi_by_SR * f_rate;
	int iSlow10 = int(k_iSR_by_2 / f_rate);
	double fSlow11 = 1.0 / double(iSlow10);
	for (int i0 = 0; i0 < count; i0 = i0 + 1) {
		iVec0[0] = 1;
		double fTemp_cds_y0 = f_cds_y[1] * (1.0 - k_iSR / (k_iSR + 0.06 * std::exp(0.0 - 2.4849066497880004 * f_cds_y[1])));
		fRec1[0] = fSlow8 + (fRec1[1] - std::floor(fSlow8 + fRec1[1]));
		fRec4[0] = fRec4[1] + fSlow9 * (0.0 - fRec2[1]);
		fRec3[0] = fSlow9 * fRec4[0] + double(1 - iVec0[1]) + fRec3[1];
		fRec2[0] = fRec3[0];
		iRec6[0] = ((iRec6[1] > 0) ? 2 * (iRec5[1] < iSlow10) + -1 : 1 - 2 * (iRec5[1] > 0));
		iRec5[0] = iRec6[0] + iRec5[1];
        double xValue = 
                (b_triangle) ? fSlow11 * double(iRec5[0]) 
                :   ((b_sin) 
                    ? std::max<double>(0.0, 0.5 * (fRec2[0] + 1.0)) 
                    : double(fRec1[0] <= 0.5)
                );
        double xPow =  std::pow(1.0 - f_depth * (1.0 - (
                xValue
            )), 1.9);
		f_cds_y[0] = fTemp_cds_y0 + k_iSR * (
            xPow / (k_iSR + 0.06 * std::exp(0.0 - 2.4849066497880004 * fTemp_cds_y0
                    )
        ));
		output0[i0] = FAUSTFLOAT(double(input0[i0]) * (f_dry + f_wet_x_R1 /
            (std::exp(13.815510557964274 / std::log(8.551967507929417 * f_cds_y[0] + 2.718281828459045)) + 2.7e+03)));
		iVec0[1] = iVec0[0];
		fRec1[1] = fRec1[0];
		fRec4[1] = fRec4[0];
		fRec3[1] = fRec3[0];
		fRec2[1] = fRec2[0];
		iRec6[1] = iRec6[0];
		iRec5[1] = iRec5[0];
		f_cds_y[1] = f_cds_y[0];
	}
}


} // end namespace tremolo
