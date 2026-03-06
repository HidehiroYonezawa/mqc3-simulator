#pragma once

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <unordered_map>

#include "bosim/base/log.h"

#include "mqc3_cloud/common/v1/error_detail.pb.h"

template <class T>
struct is_fmt_named_arg_char : std::false_type {};  // NOLINT
template <class T>
struct is_fmt_named_arg_char<fmt::detail::named_arg<char, T>> : std::true_type {};
template <class T>
inline constexpr bool is_fmt_named_arg_char_v =  // NOLINT
    is_fmt_named_arg_char<std::remove_cvref_t<T>>::value;
template <class T>
concept FmtNamedArgChar = is_fmt_named_arg_char_v<T>;

template <FmtNamedArgChar... NamedArgs>
std::string FormatLikePython(std::string_view fmt_str, NamedArgs&&... named) {
    return fmt::format(fmt::runtime(fmt_str), std::forward<NamedArgs>(named)...);
}

struct StatusMessage {
    std::string code;
    std::string message;

    void ToError(mqc3_cloud::common::v1::ErrorDetail* o_ret) const {
        o_ret->set_code(code);
        o_ret->set_description(message);
    }
    [[nodiscard]] mqc3_cloud::common::v1::ErrorDetail ToError() const {
        auto ret = mqc3_cloud::common::v1::ErrorDetail();
        ret.set_code(code);
        ret.set_description(message);
        return ret;
    }
};

class MessageManager {
public:
    MessageManager(const MessageManager&) = delete;
    MessageManager& operator=(const MessageManager&) = delete;
    MessageManager(MessageManager&&) = delete;
    MessageManager& operator=(MessageManager&&) = delete;

    template <FmtNamedArgChar... NamedArgs>
    static StatusMessage GetMessage(const std::string& key, NamedArgs&&... named) {
        const auto itr = GetMessageManager()->dict_.find(key);
        if (itr == GetMessageManager()->dict_.end()) {
            LOG_FATAL("Unknown key: {}", key);
            return {.code = "INTERNAL_ERROR",
                    .message = "Critical internal error. The issue has been logged."};
        }

        return {
            .code = itr->second.code,
            .message = FormatLikePython(itr->second.message, std::forward<NamedArgs>(named)...)};
    }

    static void InitializeFromFile(std::string_view file) {
        auto ifs = std::ifstream(file.data());  // NOLINT
        auto j = nlohmann::json();
        ifs >> j;
        InitializeFromJson(j);
    }
    static void InitializeFromJson(const nlohmann::json& j) {
        for (const auto& [key, sm] : j.items()) {
            GetMessageManager()->dict_[key] = {.code = sm["code"], .message = sm["message"]};
        }
    }

private:
    MessageManager() = default;
    ~MessageManager() = default;

    static MessageManager* GetMessageManager();
    std::unordered_map<std::string, StatusMessage> dict_;
};
