#pragma once

#include "server/clients/base.h"

#include "mqc3_cloud/scheduler/v1/execution.grpc.pb.h"
#include "mqc3_cloud/scheduler/v1/execution.pb.h"

class ExecutionClient : public rpc::BaseGrpcClient {
public:
    explicit ExecutionClient(const rpc::GrpcClientSettings& settings)
        : rpc::BaseGrpcClient(settings) {
        // Safe: base is fully constructed now.
        InitializeStub();
    }

    auto AssignNextJob(const mqc3_cloud::scheduler::v1::AssignNextJobRequest& req) {
        using Response = mqc3_cloud::scheduler::v1::AssignNextJobResponse;
        return CallRpcOrThrow<Response>([this, &req](grpc::ClientContext& ctx, Response& out) {
            return stub_->AssignNextJob(&ctx, req, &out);
        });
    }
    auto ReportExecutionResult(const mqc3_cloud::scheduler::v1::ReportExecutionResultRequest& req) {
        using Response = mqc3_cloud::scheduler::v1::ReportExecutionResultResponse;
        return CallRpcOrThrow<Response>([this, &req](grpc::ClientContext& ctx, Response& out) {
            return stub_->ReportExecutionResult(&ctx, req, &out);
        });
    }
    auto RefreshUploadUrl(const mqc3_cloud::scheduler::v1::RefreshUploadUrlRequest& req) {
        using Response = mqc3_cloud::scheduler::v1::RefreshUploadUrlResponse;
        return CallRpcOrThrow<Response>([this, &req](grpc::ClientContext& ctx, Response& out) {
            return stub_->RefreshUploadUrl(&ctx, req, &out);
        });
    }

protected:
    void RecreateStubImpl() override {
        stub_ = mqc3_cloud::scheduler::v1::ExecutionService::NewStub(Channel());
    }

private:
    std::unique_ptr<mqc3_cloud::scheduler::v1::ExecutionService::Stub> stub_;
};
