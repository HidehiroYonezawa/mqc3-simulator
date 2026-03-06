#include "bosim/base/log.h"

namespace bosim {
Logger* Logger::GetLogger() {
    static Logger Logger;
    return &Logger;
}
}  // namespace bosim
