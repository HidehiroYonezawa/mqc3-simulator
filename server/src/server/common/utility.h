#pragma once

#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/util/time_util.h>

// Forward declarations
class Duration;

using TimeUtil = google::protobuf::util::TimeUtil;

class Timestamp {
public:
    explicit Timestamp(const google::protobuf::Timestamp& data) : data_{data} {}

    static Timestamp Now() { return Timestamp(TimeUtil::GetCurrentTime()); }

    [[nodiscard]] std::int64_t Seconds() const { return data_.seconds(); }
    [[nodiscard]] std::int64_t Nanos() const { return data_.nanos(); }

    [[nodiscard]] std::string ToString() const { return data_.DebugString(); }

    [[nodiscard]] bool IsInitialized() const { return data_.IsInitialized(); }
    [[nodiscard]] bool IsValid() const { return TimeUtil::IsTimestampValid(data_); }

    [[nodiscard]] const google::protobuf::Timestamp& Data() const { return data_; }

    Timestamp& operator+=(const Duration& d);

private:
    friend class Duration;

    Timestamp() = default;

    google::protobuf::Timestamp data_;
};

#pragma region Compare Timestamp
inline bool operator==(const Timestamp& lhs, const Timestamp& rhs) {
    return lhs.Seconds() == rhs.Seconds() && lhs.Nanos() == rhs.Nanos();
}
inline bool operator!=(const Timestamp& lhs, const Timestamp& rhs) { return !(lhs == rhs); }
inline bool operator<(const Timestamp& lhs, const Timestamp& rhs) {
    if (lhs.Seconds() < rhs.Seconds()) { return true; }
    if (lhs.Seconds() == rhs.Seconds() && lhs.Nanos() < rhs.Nanos()) { return true; }
    return false;
}
inline bool operator<=(const Timestamp& lhs, const Timestamp& rhs) {
    if (lhs.Seconds() < rhs.Seconds()) { return true; }
    if (lhs.Seconds() == rhs.Seconds() && lhs.Nanos() <= rhs.Nanos()) { return true; }
    return false;
}
inline bool operator>(const Timestamp& lhs, const Timestamp& rhs) { return !(lhs <= rhs); }
inline bool operator>=(const Timestamp& lhs, const Timestamp& rhs) { return !(lhs < rhs); }
#pragma endregion

class Duration {
public:
    explicit Duration(const google::protobuf::Duration& data) : data_{data} {}
    explicit Duration(double seconds)
        : data_{TimeUtil::NanosecondsToDuration(
              static_cast<std::int64_t>(seconds * 1'000'000'000))} {}
    Duration(const Timestamp& started_at, const Timestamp& finished_at)
        : data_{TimeUtil::NanosecondsToDuration(
              TimeUtil::TimestampToNanoseconds(finished_at.data_) -
              TimeUtil::TimestampToNanoseconds(started_at.data_))} {}

    [[nodiscard]] double Seconds() const {
        return static_cast<double>(TimeUtil::DurationToNanoseconds(data_)) / 1'000'000'000;
    }
    [[nodiscard]] std::int64_t Nanos() const { return TimeUtil::DurationToNanoseconds(data_); }

    [[nodiscard]] std::string ToString() const { return data_.DebugString(); }

    [[nodiscard]] bool IsValid() const { return TimeUtil::IsDurationValid(data_); }
    [[nodiscard]] bool IsInitialized() const { return data_.IsInitialized(); }

    [[nodiscard]] const google::protobuf::Duration& Data() const { return data_; }

private:
    friend class Timestamp;

    Duration() = default;

    google::protobuf::Duration data_;
};

#pragma region Compare Duration
inline bool operator==(const Duration& lhs, const Duration& rhs) {
    return lhs.Nanos() == rhs.Nanos();
}
inline bool operator!=(const Duration& lhs, const Duration& rhs) { return !(lhs == rhs); }
inline bool operator<(const Duration& lhs, const Duration& rhs) {
    return lhs.Nanos() < rhs.Nanos();
}
inline bool operator<=(const Duration& lhs, const Duration& rhs) {
    return lhs.Nanos() <= rhs.Nanos();
}
inline bool operator>(const Duration& lhs, const Duration& rhs) { return !(lhs <= rhs); }
inline bool operator>=(const Duration& lhs, const Duration& rhs) { return !(lhs < rhs); }
#pragma endregion

Duration operator-(const Timestamp& lhs, const Timestamp& rhs);
Timestamp operator+(const Timestamp& lhs, const Duration& rhs);
