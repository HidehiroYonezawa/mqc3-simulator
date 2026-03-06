#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "bosim/base/log.h"

namespace rpc {

// ----------------------------- Exceptions ----------------------------------

struct GrpcError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct TimeoutError : GrpcError {
    using GrpcError::GrpcError;
};
struct ConnectionError : GrpcError {
    using GrpcError::GrpcError;
};
struct NotFoundError : GrpcError {
    using GrpcError::GrpcError;
};

// ------------------------------- Settings ----------------------------------

struct GrpcClientSettings {
    std::string address;
    std::int32_t max_send_message_length;
    std::int32_t max_receive_message_length;
    std::uint32_t max_retry_count;  // total attempts = max_retry_count (reconnect before final)
    std::chrono::milliseconds timeout;
    std::optional<std::string> ca_cert_file;  // std::nullopt => insecure channel
};

// --------------------------- BaseGrpcClient --------------------------------

class BaseGrpcClient {
public:
    explicit BaseGrpcClient(const GrpcClientSettings& settings)
        : address_(settings.address),
          max_retry_count_(settings.max_retry_count > 0 ? settings.max_retry_count : 1),
          timeout_(settings.timeout), ca_cert_file_(settings.ca_cert_file) {
        grpc::ChannelArguments args;
        args.SetMaxSendMessageSize(settings.max_send_message_length);
        args.SetMaxReceiveMessageSize(settings.max_receive_message_length);
        channel_args_ = args;

        if (ca_cert_file_) {
            ca_cert_ = ReadFileImpl(*ca_cert_file_);
            if (ca_cert_.empty()) {
                throw std::runtime_error("CA certificate file is empty or unreadable: " +
                                         *ca_cert_file_);
            }
            grpc::SslCredentialsOptions ssl_opts;
            ssl_opts.pem_root_certs = ca_cert_;
            creds_ = grpc::SslCredentials(ssl_opts);
            secure_ = true;
        } else {
            creds_ = grpc::InsecureChannelCredentials();
            secure_ = false;
        }

        channel_ = CreateChannelImpl();
        // Do NOT call RecreateStubImpl() here—derived is not fully constructed yet.
    }

    virtual ~BaseGrpcClient() = default;

    BaseGrpcClient(const BaseGrpcClient&) = delete;
    BaseGrpcClient& operator=(const BaseGrpcClient&) = delete;

protected:
    // Derived must implement stub recreation from `channel_`.
    virtual void RecreateStubImpl() = 0;

    // Derived must call this in its constructor body after BaseGrpcClient constructed.
    void InitializeStub() {
        RecreateStubImpl();
        stub_initialized_.store(true, std::memory_order_release);
    }

    const std::shared_ptr<grpc::Channel>& Channel() const { return channel_; }
    const std::string& Address() const { return address_; }
    bool IsSecure() const { return secure_; }

    void Reconnect() {
        if (!stub_initialized_.load(std::memory_order_acquire)) {
            throw std::logic_error("Reconnect() called before InitializeStub() in derived class");
        }
        LOG_WARNING("Reconnecting gRPC channel...");
        channel_ = CreateChannelImpl();
        RecreateStubImpl();  // safe: derived is fully constructed at this point
    }

    // Unary RPC wrapper: returns a Response by value.
    template <typename Response>
    Response CallRpcOrThrow(
        const std::function<grpc::Status(grpc::ClientContext&, Response&)>& invoke,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        Response out;
        CallRpcOrThrowImpl<Response>(invoke, out, timeout);
        return out;
    }

    // Unary RPC wrapper: writes into an existing Response object.
    template <typename Response>
    void CallRpcOrThrow(const std::function<grpc::Status(grpc::ClientContext&, Response&)>& invoke,
                        Response& out,
                        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        CallRpcOrThrowImpl<Response>(invoke, out, timeout);
    }

    // Public-style hook name you requested; delegates to Impl helpers.
    static void HandleRpcError(const grpc::Status& s, bool log_only) {
        LogRpcErrorImpl(s, log_only);
        if (!log_only) { ThrowMappedErrorImpl(s); }
    }

private:
    std::shared_ptr<grpc::Channel> CreateChannelImpl() {
        return grpc::CreateCustomChannel(address_, creds_, channel_args_);
    }

    static void SetDeadlineImpl(grpc::ClientContext& ctx, std::chrono::milliseconds timeout) {
        ctx.set_deadline(std::chrono::system_clock::now() + timeout);
    }

    static bool IsRetryableImpl(const grpc::Status& s) {
        const auto code = s.error_code();
        return code == grpc::StatusCode::UNAVAILABLE || code == grpc::StatusCode::DEADLINE_EXCEEDED;
    }

    static const char* StatusCodeNameImpl(grpc::StatusCode code) noexcept {
        switch (code) {
            case grpc::StatusCode::OK: return "OK";
            case grpc::StatusCode::CANCELLED: return "CANCELLED";
            case grpc::StatusCode::UNKNOWN: return "UNKNOWN";
            case grpc::StatusCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
            case grpc::StatusCode::DEADLINE_EXCEEDED: return "DEADLINE_EXCEEDED";
            case grpc::StatusCode::NOT_FOUND: return "NOT_FOUND";
            case grpc::StatusCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
            case grpc::StatusCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
            case grpc::StatusCode::UNAUTHENTICATED: return "UNAUTHENTICATED";
            case grpc::StatusCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
            case grpc::StatusCode::FAILED_PRECONDITION: return "FAILED_PRECONDITION";
            case grpc::StatusCode::ABORTED: return "ABORTED";
            case grpc::StatusCode::OUT_OF_RANGE: return "OUT_OF_RANGE";
            case grpc::StatusCode::UNIMPLEMENTED: return "UNIMPLEMENTED";
            case grpc::StatusCode::INTERNAL: return "INTERNAL";
            case grpc::StatusCode::UNAVAILABLE: return "UNAVAILABLE";
            case grpc::StatusCode::DATA_LOSS: return "DATA_LOSS";
            default: return "UNKNOWN_STATUS_CODE";
        }
    }

    static void LogRpcErrorImpl(const grpc::Status& s, bool log_only) {
        const auto code = s.error_code();
        const auto& msg = s.error_message();
        const char* code_name = StatusCodeNameImpl(code);
        if (log_only) {
            LOG_WARNING("Error during RPC: {} ({})", msg, code_name);
        } else {
            LOG_ERROR("Error during RPC: {} ({})", msg, code_name);
        }
    }

    [[noreturn]] static void ThrowMappedErrorImpl(const grpc::Status& s) {
        const auto code = s.error_code();
        const auto& msg = s.error_message();
        switch (code) {
            case grpc::StatusCode::DEADLINE_EXCEEDED:
                throw TimeoutError("gRPC request timed out: " + msg);
            case grpc::StatusCode::UNAVAILABLE:
                throw ConnectionError("Unable to connect to gRPC server: " + msg);
            case grpc::StatusCode::NOT_FOUND:
                throw NotFoundError("Requested resource not found: " + msg);
            default:
                throw GrpcError(std::string("Unexpected gRPC error: ") + StatusCodeNameImpl(code) +
                                " - " + msg);
        }
    }

    template <typename Response>
    void CallRpcOrThrowImpl(
        const std::function<grpc::Status(grpc::ClientContext&, Response&)>& invoke, Response& out,
        std::optional<std::chrono::milliseconds> timeout) {
        if (!stub_initialized_.load(std::memory_order_acquire)) {
            throw std::logic_error(
                "Stub not initialized. Call InitializeStub() in derived ctor body.");
        }

        const auto to = timeout.value_or(timeout_);
        const auto attempts_before_reconnect = (max_retry_count_ > 0) ? (max_retry_count_ - 1) : 0;

        for (std::uint32_t attempt = 0; attempt < attempts_before_reconnect; ++attempt) {
            grpc::ClientContext ctx;
            SetDeadlineImpl(ctx, to);
            const grpc::Status s = invoke(ctx, out);
            if (s.ok()) return;

            if (IsRetryableImpl(s)) {
                HandleRpcError(s, /*log_only=*/true);
            } else {
                HandleRpcError(s, /*log_only=*/false);  // throws
            }
        }

        if (attempts_before_reconnect > 0) { Reconnect(); }

        grpc::ClientContext ctx;
        SetDeadlineImpl(ctx, to);
        const grpc::Status s = invoke(ctx, out);
        if (s.ok()) return;

        HandleRpcError(s, /*log_only=*/false);  // throws
    }

    static std::string ReadFileImpl(const std::string& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return {};
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }

protected:
    std::string address_;
    std::uint32_t max_retry_count_;
    std::chrono::milliseconds timeout_;
    std::optional<std::string> ca_cert_file_;
    std::string ca_cert_;
    bool secure_ = false;

    grpc::ChannelArguments channel_args_;
    std::shared_ptr<grpc::ChannelCredentials> creds_;
    std::shared_ptr<grpc::Channel> channel_;

private:
    std::atomic<bool> stub_initialized_{false};
};

}  // namespace rpc
