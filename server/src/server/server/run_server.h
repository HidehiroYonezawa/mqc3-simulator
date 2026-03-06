#pragma once

#include <cstdint>

#include "server/settings/settings.h"

std::int32_t RunGRPCServer(const Settings& settings);
