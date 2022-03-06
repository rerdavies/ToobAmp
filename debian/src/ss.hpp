#pragma once

#include <sstream>

// usage  SS(a << b << "abc" << 0.1) 
// returns std::string&&
#define SS(x) ( ((std::stringstream&)(std::stringstream() << x )).str())