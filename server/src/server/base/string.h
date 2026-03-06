#pragma once

#include <string_view>

inline std::string_view RemovePeriod(std::string_view s) {
    if (s.ends_with('.')) { return s.substr(0, s.size() - 1); }
    return s;
}
