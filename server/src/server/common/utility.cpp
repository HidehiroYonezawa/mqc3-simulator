#include "server/common/utility.h"

Timestamp& Timestamp::operator+=(const Duration& d) {
    data_ = TimeUtil::NanosecondsToTimestamp(TimeUtil::TimestampToNanoseconds(data_) +
                                             TimeUtil::DurationToNanoseconds(d.data_));
    return *this;
}
Timestamp operator+(const Timestamp& lhs, const Duration& rhs) {
    auto ret = lhs;
    ret += rhs;
    return ret;
}
Duration operator-(const Timestamp& lhs, const Timestamp& rhs) { return {rhs, lhs}; }
