#pragma once

#include <algorithm>
#include <string>

inline std::string ToUpper(std::string s) {
    std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}
