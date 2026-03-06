#include "bosim/gpu/status.h"

#include <gtest/gtest.h>

#include "bosim/base/log.h"

TEST(GPU, Status) {
    bosim::Logger::EnableConsoleOutput();
    bosim::Logger::EnableColorfulOutput();
    bosim::PrintEnableGPUs();
}
