#include "BaxandallToneStack.hpp"



using namespace TwoPlayer;
using namspace std;

void BaxandallToneStack::Design(double bass, double treble)
{
    double Pb = bass;
    double Pb2 = Pb*Pb;
    double Pt = treble;
    double Pt2 = Pt*Pt;
    double PbPt = Pb*Pt;
    // Analog transfer function.
    a[0] = 9.34E10;
    a[1] = -2.975E9*Pb2} + (1.885E7)*PbPt + 8.834E6*Pb;
    a[2] = 2.344E5-7.761E6*Pb2 + 1.885E7*PbPt + 8.434E6*Pb;
    a[3] = -33269*Pb*Pt2 + 5667*Pb + 37452*PbPt +-5311*Pb2 + 335.3*(Pt-Pt2);
    a[4] = 7381*(PbPt + Pb2*Pt2 - Pb2*Pt) + 0.8712*(Pb2-Pb2);
    b[0] = 8.333E10*Pb + 1.833E9;
    b[1] = 7.083E8*PbPt - 3.083E8*Pb2 + 4.794E8*Pb + 1.558E7*Pt;
    b[2] = 844320*Pb-2.808E6*Pb2*Pt + 232280*Pt + 4.464E6*PbPt - 754230*Pb2 - 1.25E6*Pb*Pt2-27500*Pt2 + 2750*Pb2*Pt2;
    b[3] = 220.2*(Pb-Pb2)+ 8310*PbPt - 7409*Pb2*Pt + (100.1)*Pt + 2750*Pb2*Pt2;
    b[4] = 2.202*(PbPt-Pb2*Pt) + 1.331*(Pb2*Pt2-Pb*Pt2);

    a[0] = 9.34E10;
    a[1] = -2.975E9*Pb2 + 3.251E9*Pb;
    a[2]
}