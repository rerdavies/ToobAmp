#include "ToneStackFilter.h"
#include <cmath>

using namespace TwoPlay;

class AmpComponents 
{
public:
    AmpComponents(double c1, double c2, double c3,double r1, double r2, double r3,double r4)
    {
        C1 = c1; C2 = c2; C3 = c3;
        R1 = r1; R2 = r2; R3 = r3; R4 = r4;


        B1_h = C1*R1;
        B1_m = C3*R3;
        B1_l = C1*R1+C2*R2;
        B1_c = C1*R3+C2*R3;


        B2_h = (C1*C2 + C1*C3)* R1*R4;
        B2_m2 = -(C1*C3 + C2*C3)*R3*R3;
        B2_m = (C1*C3)*(R1*R3+R3*R3) + C2*C3*R3*R3;
        B2_l = (C1*C2)*(R1*R2+R2*R4)+ C1*C3*R2*R4;
        B2_ml = (C1*C3+C2*C3)*R2*R3;
        B2_c = (C1*C2)*(R1*R3+R3*R4) + (C1*C3)*R3*R4;

        B3_lm = (R1*R2*R3 + R2*R3*R4);
        B3_m2 = -(R3*R3)*(R1+R4);
        B3_m = R3*R3*(R1+R4);
        B3_h = R1*R3*R4;
        B3_mh = -R1*R3*R4;
        B3_lh = R1*R2*R4;
        B3_Scale = C1*C2*C3;

        A1_c = (C1*R1+C1*R3 + C2*R3+C3*R4) + C3*R3;
        A1_m = C3*R3;
        A1_l = C1*R2+C2*R2;

        A2_m = C1*C2*R1*R3
            - (C2*C3*R3*R4)
            + (C2*C3+C1*C3)*R3*R3;
        A2_lm = (C1*C3+ C2*C3)*R2*R3;
        A2_m2 = -(C1*C3+C2*C3)*R3*R3;
        A2_l = (C1*C2)*(R1*R2+R2*R4)+(C1*C3+C2*C3)*R2*R4;
        A2_c = C1*C2*(R1*R3+R1*R4+R3*R4)+C1*C3*(R1*R4+R3*R4) + C2*C3*R3*R4;


        A3_lm = R2*R3 * (R1+R4);
        A3_m2 = -R3*R3*(R1+R4);
        A3_m = R3*R3*(R1+R4)-R1*R3*R4;
        A3_l = R1*R2*R4;
        A3_c = R1*R3*R4;
        A3_Scale = C1*C2*C3;




    }
private:

    double A1_c,A1_m, A1_l;
    double A2_m, A2_lm,A2_m2,A2_l, A2_c;
    double A3_lm,A3_m2,A3_m,A3_l,A3_c, A3_Scale;

    double B1_h, B1_m, B1_l,B1_c;
    double B2_h, B2_m2, B2_m, B2_l,B2_ml,B2_c;
    double B3_lm, B3_m2, B3_m,B3_h,B3_mh,B3_lh, B3_Scale;


    double C1,C2,C3;
    double R1,R2,R3,R4;

public:
    double b0(double l, double m, double h) { UNUSED(l); UNUSED(m); UNUSED(h); return 0; }
    double b1(double l, double m, double h) { 

        return B1_h*h
            + B1_m *m
            + B1_l *l
            + B1_c;
    }
    double b2(double l, double m, double h)
    {

        return B2_h*h
            +   B2_m2*m*m
            +   B2_m*m
            +   B2_l*l
            +   B2_ml*m*l
            +   B2_c
            ;
    }
    double b3(double l, double m, double h)
    {
        return (B3_lm*l*m
            +  B3_m2 * m*m
            + B3_m * m
            + B3_h * h
            + B3_mh * m * h
            + B3_lh * l * h)*B3_Scale;



    }

    double a0(double l, double m, double h)
    {
        UNUSED(l); UNUSED(m); UNUSED(h);
        return 1;
    }
    double a1(double l, double m, double h)
    {
        UNUSED(h);
        return A1_c + A1_m*m + A1_l*l;
    }
    double a2(double l, double m, double h)
    {
        UNUSED(h);
        return A2_m*m
            +  A2_lm*l*m
            +  A2_m2*m*m
            +  A2_l * l
            +  A2_c
            ;
    }

    double a3(double l, double m, double h)
    {
        UNUSED(h);
        return (A3_lm*l*m
        + A3_m2*m*m
        + A3_m*m
        + A3_l*l
        + A3_c)*A3_Scale;
    }

};

static AmpComponents bassmanComponents = AmpComponents(
    2.5e-10,2e-8,2E-8,
    250000,
    1E6,
    25000,
    45000
);

static AmpComponents jcmComponents = {
    4.7e-10,2.2e-8,2.2e-8,
    220000,
    1E6,
    22000,
    33000
};


// 0.3 out at 0.5. 0.09 at 0
static  double B_CONSTANT = std::log(0.3);

// non-standard fender taper.
static double AudioTaperB(double value)
{
    return exp((2-2*value)*B_CONSTANT);

}


// 0.1 out at 0.5. 0.01 at 0
static  double A_CONSTANT = std::log(0.1);

// standard audio taper (jcm8000)
static double AudioTaperA(double value)
{
    return exp((2-2*value)*A_CONSTANT);

}
void ToneStackFilter::BilinearTransform(float frequency, const FilterCoefficients3& prototype, FilterCoefficients3* result)
{
	double w0 = frequency *(2*M_PI);
	double k = w0 / std::tan(w0 * T * 0.5);
	double k2 = k * k;
	double k3 = k2*k;


    // Bilinear transform + causitive form conversion.


	double b0 = prototype.b[0] + prototype.b[1] * k + prototype.b[2] * k2 + prototype.b[3] * k3;
	double b1 = 3 * prototype.b[0] + prototype.b[1] * k - prototype.b[2] * k2 -3* prototype.b[3] * k3;
	double b2 = 3* prototype.b[0] - prototype.b[1] * k - prototype.b[2] * k2 + 3*prototype.b[3] * k3;
	double b3 = prototype.b[0] - prototype.b[1] * k + prototype.b[2] * k2 - prototype.b[3] * k3;

	double a0 = prototype.a[0] + prototype.a[1] * k + prototype.a[2] * k2 + prototype.a[3] * k3;
	double a1 = 3 * prototype.a[0] + prototype.a[1] * k - prototype.a[2] * k2 -3* prototype.a[3] * k3;
	double a2 = 3 *prototype.a[0] - prototype.a[1] * k - prototype.a[2] * k2 +3* prototype.a[3] * k3;
	double a3 = prototype.a[0] - prototype.a[1] * k + prototype.a[2] * k2 - prototype.a[3] * k3;

	// normalize.
	double scale = 1.0 / a0;

	result->a[0] = a0 * scale;
	result->a[1] = a1 * scale;
	result->a[2] = a2 * scale;
	result->a[3] = a3 * scale;
	result->b[0] = b0 * scale;
	result->b[1] = b1 * scale;
	result->b[2] = b2 * scale;
	result->b[3] = b3 * scale;

}


void ToneStackFilter::UpdateFilter()
{
    float l = Bass.GetValue();
    float m = Mid.GetValue();
    float h = Treble.GetValue();
    bool isBassman = AmpModel.GetValue() < 0.5f;
    AmpComponents *c;
    if (isBassman) {
        l = AudioTaperB(l);
        c = &bassmanComponents;
    } else {
        l = AudioTaperA(l);
        c = & jcmComponents;
    }
    prototype.b[0] = c->b0(l,m,h);
    prototype.b[1] = c->b1(l,m,h);
    prototype.b[2] = c->b2(l,m,h);
    prototype.b[3] = c->b3(l,m,h);
    prototype.a[0] = c->a0(l,m,h);
    prototype.a[1] = c->a1(l,m,h);
    prototype.a[2] = c->a2(l,m,h);
    prototype.a[3] = c->a3(l,m,h);

    BilinearTransform(1000.0,prototype,&this->zTransformCoefficients);

}