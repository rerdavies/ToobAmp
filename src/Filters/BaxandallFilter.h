#pragma once

// based on ampbooks.com/mobile/dsp/tonestack


namespace TwoPlayer {


class BaxandallToneStack {

private:
    void Design(double bass, double treble);
    double a[5];
    double b[5];

};

} // namespace