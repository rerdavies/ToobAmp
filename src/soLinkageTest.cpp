#include <stdlib.h>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>

using namespace std;

// Just make sure that ToobAmp.so isn't missing any linkages.
// If we run successfully, everything's ok.


    extern "C" {
        const void*
        lv2_descriptor(uint32_t index);
    }


int main(int argc, char**argv)
{

    std::filesystem::path soName = std::filesystem::path(argv[0]).parent_path() / "ToobAmp.so";

    soName = "/usr/lib/lv2/ToobAmp.lv2/ToobAmp.so";
    if (!std::filesystem::exists(soName))
    {
        cerr << "File not found: " << soName << endl;
        return EXIT_FAILURE;
    }

    auto handle = dlopen(soName.c_str(),RTLD_NOW );
    if (!handle)
    {
        cerr << "Failed to load. " <<  dlerror() << endl;
        return EXIT_FAILURE;
    }
    
    void *entryPoint = dlsym(handle, "lv2_descriptor");
    if (entryPoint == nullptr)
    {
        cerr << "Entry point not found." << endl;
        return EXIT_FAILURE;

    }

    using Lv2Fn = void*(*)(uint32_t v);
    Lv2Fn fn = Lv2Fn(entryPoint);
    fn(0);

    
    return EXIT_SUCCESS;
}