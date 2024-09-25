#include "LsNumerics/Freeverb.hpp"
#include <cstdint>
#include <iostream>

// Just make sure that ToobAmp.so isn't missing any linkages.
// If we built successfully, everything's ok.


extern "C" {
    const void*
        lv2_descriptor(uint32_t index);
};


int main(int argc, char**argv)
{
    // we're just testing for linkage, that's it.
    if (&lv2_descriptor < (void*)1)
    {
        std::cout << "Error." << std::endl;
    }
    return 0;
}