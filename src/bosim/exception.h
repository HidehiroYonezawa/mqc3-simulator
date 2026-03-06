#pragma once

#include <cstdint>
#include <exception>
#include <string>

namespace bosim {
class Exception : public std::exception {
public:
    explicit Exception(std::int32_t id_, const std::string& msg) : id{id_}, msg_{msg} {}
    ~Exception() noexcept override = default;

    const std::int32_t id;
    const char* what() const noexcept override { return msg_.c_str(); }

private:
    std::string msg_;
};

class ParseError : public Exception {
public:
    using Exception::Exception;
};

class SimulationError : public Exception {
public:
    using Exception::Exception;
};

class OutOfRange : public Exception {
public:
    using Exception::Exception;
};

class OtherError : public Exception {
public:
    using Exception::Exception;
};

namespace error {
// ParseError
static constexpr std::int32_t FileNotFound = 101;

// SimulationError
static constexpr std::int32_t InvalidSettings = 201;
static constexpr std::int32_t InvalidOperation = 202;
static constexpr std::int32_t InvalidState = 203;
static constexpr std::int32_t InvalidFF = 204;
static constexpr std::int32_t Overflow = 205;
static constexpr std::int32_t NaN = 206;
static constexpr std::int32_t Timeout = 207;
static constexpr std::int32_t Unknown = 299;

// OutOfRange
static constexpr std::int32_t LargeIndex = 301;
static constexpr std::int32_t SmallIndex = 302;
static constexpr std::int32_t KeyNotFound = 303;

// OtherError
static constexpr std::int32_t Unreachable = 501;
}  // namespace error
}  // namespace bosim
