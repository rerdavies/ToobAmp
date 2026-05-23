/*
MIT License

Copyright (c) 2026 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
    dynamically links to either toob ToobAmp-a72.so or ToobAmp-a76.so depending on which architecture we're using.
*/

#include <cstdint>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <string>
#include <iostream>
#include <vector>
#include <string>
#include "lv2/core/lv2.h"
#include <dlfcn.h>
#include <memory.h>
#include "X86ProcessorCheck.hpp"

using namespace toob;

typedef const LV2_Descriptor *
EntryPointT(uint32_t index);

static void *dlHandle = nullptr;
static EntryPointT *arch_lv2_descriptor = nullptr;

void findArchEntryPoint()
{
    if (arch_lv2_descriptor != nullptr)
    {
        return;
    }

    CpuLevel cpuLevel = GetX86CpuLevel();

    // Get the directory of the current .so library
    Dl_info dl_info;
    memset(&dl_info,0,sizeof(dl_info));
    if (dladdr((void *)findArchEntryPoint, &dl_info) == 0 || dl_info.dli_fname == nullptr)
    {
        return;
    }
    std::filesystem::path libPath = dl_info.dli_fname;

    // not static, because C++ runtime may not be fully initialized.
    std::vector<std::string> soNames {
        "ToobAmp-x86-64.so",
        "ToobAmp-x86-64-sse42.so",
        "ToobAmp-x86-64-avx.so",
        "ToobAmp-x86-64-avx512.so",

    };
    std::filesystem::path dynamicLibPath;
    while (true) 
    {
        std::string soName = soNames[(size_t)cpuLevel];
        dynamicLibPath = libPath.parent_path() / "bin" / soName;
        if (std::filesystem::exists(dynamicLibPath)) 
        {
            break;
        }
        if (cpuLevel == CpuLevel::V1)
        {
            break;
        }
        cpuLevel = (CpuLevel)((int)cpuLevel-1);
    }

    dlHandle = dlopen(dynamicLibPath.c_str(), RTLD_LAZY);
    if (!dlHandle)
    {
        std::cout << "Cannot load library: " << dlerror() << std::endl;
        return;
    }
    arch_lv2_descriptor = (EntryPointT *)dlsym(dlHandle, "lv2_descriptor");
    if (!arch_lv2_descriptor)
    {
        std::cout << "Cannot find symbol: " << dlerror() << std::endl;
        dlclose(dlHandle);
        dlHandle = nullptr;
        return;
    }
}

static void cleanup() __attribute__((destructor));

static void cleanup()
{
    if (dlHandle)
    {
        dlclose(dlHandle);
        dlHandle = nullptr;
        arch_lv2_descriptor = nullptr;
    }
}

extern "C"
{
    LV2_SYMBOL_EXPORT
    const LV2_Descriptor *
    lv2_descriptor(uint32_t index)
    {
        findArchEntryPoint();
        if (arch_lv2_descriptor  != nullptr)
        {
            return arch_lv2_descriptor(index);
        }

        return nullptr;
    }
}
