#pragma once

#include "lv2/core/lv2.h"
#include <string>
#include <filesystem>


typedef const LV2_Descriptor* FN_LV2_ENTRY(uint32_t index);


FN_LV2_ENTRY* LoadLv2Plugin(const char* name);

std::string LocateLv2Plugin(const char* name);



