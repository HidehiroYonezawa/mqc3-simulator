#pragma once

#include <Eigen/Sparse>
#include <boost/math/distributions/normal.hpp>
#include <fmt/format.h>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "bosim/base/float.h"
#include "bosim/base/log.h"
#include "bosim/base/math.h"
#include "bosim/exception.h"
#include "bosim/state.h"

namespace bosim::operation {
template <std::floating_point Float, typename ReturnType>
class Operation {
public:
    virtual ~Operation() = default;
    virtual ReturnType operator()(State<Float>& state) const = 0;

    virtual std::string ToString() const = 0;
    virtual std::vector<std::size_t> Modes() const = 0;
    virtual std::vector<Float> Params() const { return {}; };
    virtual Float& GetMutParam([[maybe_unused]] const std::size_t idx) {
        throw std::invalid_argument("Invalid 'idx'");
    }

protected:
    // Uncomment the next line only if necessary.
    // std::optional<std::vector<std::vector<Float>>> noise_;
};

template <std::floating_point Float>
class Displacement : public Operation<Float, void> {
public:
    explicit Displacement(const std::size_t mode, const Float x, const Float p)
        : mode_(mode), x_(x), p_(p) {}

    explicit Displacement(const std::vector<std::size_t>& modes, const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 2) { throw std::invalid_argument("Size of 'params' must be 2"); }
        mode_ = modes[0];
        x_ = params[0];
        p_ = params[1];
    }

    void operator()(State<Float>& state) const override {
        this->CheckModeIsInState(state);
        this->UpdateState(state);
    }
    void operator()(State<Float>& state, const std::size_t peak_idx) const {
        this->CheckModeIsInState(state);
        this->UpdatePeak(state, peak_idx);
    }
    Float GetX() const { return x_; }
    Float& GetMutX() { return x_; }
    Float GetP() const { return p_; }
    Float& GetMutP() { return p_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return x_;
            case 1: return p_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("Displacement({},{})({})", x_, p_, mode_);
    }

    std::vector<std::size_t> Modes() const override { return {mode_}; }
    std::vector<Float> Params() const override { return {x_, p_}; }

private:
    std::size_t mode_;
    Float x_;
    Float p_;

    void CheckModeIsInState([[maybe_unused]] const State<Float>& state) const {
#ifndef NDEBUG
        if (this->mode_ >= state.NumModes()) {
            throw std::logic_error(
                "Measured mode indices must be lower than the number of modes in the state");
        }
#endif
    }

    void UpdatePeak(State<Float>& state, const std::size_t peak_idx) const {
        const auto operated_index_x = static_cast<long>(this->mode_);
        const auto operated_index_p = static_cast<long>(this->mode_ + state.NumModes());
        state.GetMutMean(peak_idx)(operated_index_x) += x_;
        state.GetMutMean(peak_idx)(operated_index_p) += p_;
    }
    void UpdateState(State<Float>& state) const {
        for (std::size_t m = 0; m < state.NumPeaks(); ++m) { UpdatePeak(state, m); }
    }
};

template <std::floating_point Float, EigenMatrix Matrix>
class UnitaryOperation : public Operation<Float, void> {
public:
    virtual const Matrix& GetOperationMatrix() const = 0;

protected:
    void UpdatePeak(State<Float>& state, const std::vector<long>& operated_indices,
                    const std::size_t peak_idx) const {
        auto& mu = state.GetMutMean(peak_idx);
        auto& sigma = state.GetMutCov(peak_idx);

        // Calculate sigma_operated and sigma_off_diag
        const MatrixD<Float> sigma_off_diag =
            GetOperationMatrix() * sigma(Eigen::all, operated_indices).transpose();
        const MatrixD<Float> sigma_operated = GetOperationMatrix() *
                                              sigma(operated_indices, operated_indices) *
                                              GetOperationMatrix().transpose();

        // Update mu and sigma
        mu(operated_indices) = GetOperationMatrix() * mu(operated_indices);
        sigma(operated_indices, Eigen::all) = sigma_off_diag;
        sigma(Eigen::all, operated_indices) = sigma_off_diag.transpose();
        sigma(operated_indices, operated_indices) = sigma_operated;
    }

    void UpdateState(State<Float>& state, const std::vector<long>& operated_indices) const {
        for (std::size_t m = 0; m < state.NumPeaks(); ++m) {
            UpdatePeak(state, operated_indices, m);
        }
    }
};

template <std::floating_point Float>
class UnaryOperation : public UnitaryOperation<Float, Matrix2D<Float>> {
public:
    explicit UnaryOperation(const std::size_t mode) : mode_(mode) {}

    void operator()(State<Float>& state) const override {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::UnaryOperation>{state.GetShot()};
#endif
        this->CheckModeIsInState(state);
        this->UpdateState(state);
    }
    void operator()(State<Float>& state, const std::size_t peak_idx) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile =
            profiler::Profiler<profiler::Section::UnaryOperationPerPeak>{state.GetShot(), peak_idx};
#endif
        this->CheckModeIsInState(state);
        this->UpdatePeak(state, peak_idx);
    }

    const Matrix2D<Float>& GetOperationMatrix() const override { return operation_matrix_; }

    void DumpToJson(const std::filesystem::path& json_path, const int indent = 4) const {
        if (json_path.extension() != ".json") {
            throw std::invalid_argument("The file does not have a .json extension: " +
                                        json_path.string());
        }

        auto json = nlohmann::ordered_json{};
        json["mode"] = mode_;
        json["matrix"] = nlohmann::ordered_json::array();
        const long size = static_cast<long>(operation_matrix_.rows());
        for (long i = 0; i < size; ++i) {
            for (long j = 0; j < size; ++j) { json["matrix"].push_back(operation_matrix_(i, j)); }
        }

        auto ofs = std::ofstream{json_path};
        ofs << json.dump(indent);
        ofs.close();
    }

    std::vector<std::size_t> Modes() const override { return {mode_}; }

protected:
    std::size_t mode_;
    Matrix2D<Float> operation_matrix_;

    void UpdatePeak(State<Float>& state, const std::size_t peak_idx) const {
        const auto operated_indices = std::vector<long>{
            {static_cast<long>(this->mode_), static_cast<long>(this->mode_ + state.NumModes())}};
        UnitaryOperation<Float, Matrix2D<Float>>::UpdatePeak(state, operated_indices, peak_idx);
    }
    void UpdateState(State<Float>& state) const {
        const auto operated_indices = std::vector<long>{
            {static_cast<long>(this->mode_), static_cast<long>(this->mode_ + state.NumModes())}};
        UnitaryOperation<Float, Matrix2D<Float>>::UpdateState(state, operated_indices);
    }

    void CheckModeIsInState([[maybe_unused]] const State<Float>& state) const {
#ifndef NDEBUG
        if (this->mode_ >= state.NumModes()) {
            throw std::logic_error(
                "Measured mode indices must be lower than the number of modes in the state");
        }
#endif
    }
};

template <std::floating_point Float>
class BinaryOperation : public UnitaryOperation<Float, MatrixD<Float>> {
public:
    explicit BinaryOperation(const std::size_t mode0, const std::size_t mode1)
        : mode0_(mode0), mode1_(mode1) {
        if (mode0 == mode1) {
            throw SimulationError(
                error::InvalidOperation,
                fmt::format("BinaryOperation cannot handle identical modes: mode={}.", mode0));
        }
    }

    void operator()(State<Float>& state) const override {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::BinaryOperation>{state.GetShot()};
#endif
        this->CheckModesAreInState(state);
        this->UpdateState(state);
    }
    void operator()(State<Float>& state, const std::size_t peak_idx) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::BinaryOperationPerPeak>{
            state.GetShot(), peak_idx};
#endif
        this->CheckModesAreInState(state);
        this->UpdatePeak(state, peak_idx);
    }

    const MatrixD<Float>& GetOperationMatrix() const override { return operation_matrix_; }

    void DumpToJson(const std::filesystem::path& json_path, const int indent = 4) const {
        if (json_path.extension() != ".json") {
            throw std::invalid_argument("The file does not have a .json extension: " +
                                        json_path.string());
        }

        auto json = nlohmann::ordered_json{};
        json["mode0"] = mode0_;
        json["mode1"] = mode1_;
        json["matrix"] = nlohmann::ordered_json::array();
        const long size = static_cast<long>(operation_matrix_.rows());
        for (long i = 0; i < size; ++i) {
            for (long j = 0; j < size; ++j) { json["matrix"].push_back(operation_matrix_(i, j)); }
        }

        auto ofs = std::ofstream{json_path};
        ofs << json.dump(indent);
        ofs.close();
    }

    std::vector<std::size_t> Modes() const override { return {mode0_, mode1_}; }

protected:
    std::size_t mode0_;
    std::size_t mode1_;
    MatrixD<Float> operation_matrix_;

    void UpdatePeak(State<Float>& state, const std::size_t peak_idx) const {
        const auto operated_indices =
            std::vector<long>{{static_cast<long>(this->mode0_), static_cast<long>(this->mode1_),
                               static_cast<long>(this->mode0_ + state.NumModes()),
                               static_cast<long>(this->mode1_ + state.NumModes())}};
        UnitaryOperation<Float, MatrixD<Float>>::UpdatePeak(state, operated_indices, peak_idx);
    }
    void UpdateState(State<Float>& state) const {
        const auto operated_indices =
            std::vector<long>{{static_cast<long>(this->mode0_), static_cast<long>(this->mode1_),
                               static_cast<long>(this->mode0_ + state.NumModes()),
                               static_cast<long>(this->mode1_ + state.NumModes())}};
        UnitaryOperation<Float, MatrixD<Float>>::UpdateState(state, operated_indices);
    }

    void CheckModesAreInState([[maybe_unused]] const State<Float>& state) const {
#ifndef NDEBUG
        if (this->mode0_ >= state.NumModes() || this->mode1_ >= state.NumModes()) {
            throw std::logic_error(
                "Operated mode indices must be lower than the number of modes in the state");
        }
#endif
    }
};

template <std::floating_point Float>
class PhaseRotation : public UnaryOperation<Float> {
public:
    explicit PhaseRotation(const std::size_t mode, const Float phi)
        : UnaryOperation<Float>(mode), phi_(phi) {
        SetMatrix();
    }

    explicit PhaseRotation(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : PhaseRotation(FromModesAndParams(modes, params)) {}

    Float GetPhi() const { return phi_; }
    Float& GetMutPhi() { return phi_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return phi_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("PhaseRotation({})({})", phi_, this->mode_);
    }
    std::vector<Float> Params() const override { return {phi_}; }

private:
    Float phi_;

    void SetMatrix() {
        this->operation_matrix_ = Matrix2D<Float>{};
        const Float s = std::sin(phi_);
        const Float c = std::cos(phi_);
        this->operation_matrix_ << c, -s, s, c;
    }
    static PhaseRotation<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                   const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 1) { throw std::invalid_argument("Size of 'params' must be 1"); }
        return PhaseRotation<Float>{modes[0], params[0]};
    }
};

template <std::floating_point Float>
class Squeezing : public UnaryOperation<Float> {
public:
    explicit Squeezing(const std::size_t mode, const Float theta)
        : UnaryOperation<Float>(mode), theta_(theta) {
        if (IsEquivModPi<Float>(theta, 0)) {
            throw SimulationError(
                error::InvalidOperation,
                "`theta` must not be an integer multiple of pi in Squeezing operation.");
        }
        SetMatrix();
    }

    explicit Squeezing(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : Squeezing(FromModesAndParams(modes, params)) {}

    Float GetTheta() const { return theta_; }
    Float& GetMutTheta() { return theta_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return theta_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("Squeezing({})({})", theta_, this->mode_);
    }
    std::vector<Float> Params() const override { return {theta_}; }

private:
    Float theta_;

    void SetMatrix() {
        this->operation_matrix_ = Matrix2D<Float>{};
        const Float t = std::tan(theta_);
        this->operation_matrix_ << 0, 1 / t, -t, 0;
    }
    static Squeezing<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                               const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 1) { throw std::invalid_argument("Size of 'params' must be 1"); }
        return Squeezing<Float>{modes[0], params[0]};
    }
};

template <std::floating_point Float>
class Squeezing45 : public UnaryOperation<Float> {
public:
    explicit Squeezing45(const std::size_t mode, const Float theta)
        : UnaryOperation<Float>(mode), theta_(theta) {
        if (IsEquivModPi<Float>(theta, 0)) {
            throw SimulationError(
                error::InvalidOperation,
                "`theta` must not be an integer multiple of pi in Squeezing45 operation.");
        }
        SetMatrix();
    }

    explicit Squeezing45(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : Squeezing45(FromModesAndParams(modes, params)) {}

    Float GetTheta() const { return theta_; }
    Float& GetMutTheta() { return theta_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return theta_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("Squeezing45({})({})", theta_, this->mode_);
    }
    std::vector<Float> Params() const override { return {theta_}; }

private:
    Float theta_;

    void SetMatrix() {
        this->operation_matrix_ = Matrix2D<Float>{};
        const Float t = std::tan(theta_);
        const Float d = (t + 1 / t) / 2;
        const Float od = (1 / t - t) / 2;
        this->operation_matrix_ << d, od, od, d;
    }
    static Squeezing45<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                 const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 1) { throw std::invalid_argument("Size of 'params' must be 1"); }
        return Squeezing45<Float>{modes[0], params[0]};
    }
};

template <std::floating_point Float>
class ShearXInvariant : public UnaryOperation<Float> {
public:
    explicit ShearXInvariant(const std::size_t mode, const Float kappa)
        : UnaryOperation<Float>(mode), kappa_(kappa) {
        SetMatrix();
    }

    explicit ShearXInvariant(const std::vector<std::size_t>& modes,
                             const std::vector<Float>& params)
        : ShearXInvariant(FromModesAndParams(modes, params)) {}

    Float GetKappa() const { return kappa_; }
    Float& GetMutKappa() { return kappa_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return kappa_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("ShearXInvariant({})({})", kappa_, this->mode_);
    }
    std::vector<Float> Params() const override { return {kappa_}; }

private:
    Float kappa_;

    void SetMatrix() {
        this->operation_matrix_ = Matrix2D<Float>{};
        this->operation_matrix_ << 1, 0, 2 * kappa_, 1;
    }
    static ShearXInvariant<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                     const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 1) { throw std::invalid_argument("Size of 'params' must be 1"); }
        return ShearXInvariant<Float>{modes[0], params[0]};
    }
};

template <std::floating_point Float>
class ShearPInvariant : public UnaryOperation<Float> {
public:
    explicit ShearPInvariant(const std::size_t mode, const Float eta)
        : UnaryOperation<Float>(mode), eta_(eta) {
        SetMatrix();
    }

    explicit ShearPInvariant(const std::vector<std::size_t>& modes,
                             const std::vector<Float>& params)
        : ShearPInvariant(FromModesAndParams(modes, params)) {}

    Float GetEta() const { return eta_; }
    Float& GetMutEta() { return eta_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return eta_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("ShearPInvariant({})({})", eta_, this->mode_);
    }
    std::vector<Float> Params() const override { return {eta_}; }

private:
    Float eta_;

    void SetMatrix() {
        this->operation_matrix_ = Matrix2D<Float>{};
        this->operation_matrix_ << 1, 2 * eta_, 0, 1;
    }
    static ShearPInvariant<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                     const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 1) { throw std::invalid_argument("Size of 'params' must be 1"); }
        return ShearPInvariant<Float>{modes[0], params[0]};
    }
};

template <std::floating_point Float>
class Arbitrary : public UnaryOperation<Float> {
public:
    explicit Arbitrary(const std::size_t mode, const Float alpha, const Float beta,
                       const Float lambda)
        : UnaryOperation<Float>(mode), alpha_(alpha), beta_(beta), lambda_(lambda) {
        SetMatrix();
    }

    explicit Arbitrary(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : Arbitrary(FromModesAndParams(modes, params)) {}

    Float GetAlpha() const { return alpha_; }
    Float& GetMutAlpha() { return alpha_; }
    Float GetBeta() const { return beta_; }
    Float& GetMutBeta() { return beta_; }
    Float GetLambda() const { return lambda_; }
    Float& GetMutLambda() { return lambda_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return alpha_;
            case 1: return beta_;
            case 2: return lambda_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("Arbitrary({},{},{})({})", alpha_, beta_, lambda_, this->mode_);
    }
    std::vector<Float> Params() const override { return {alpha_, beta_, lambda_}; }

private:
    Float alpha_;
    Float beta_;
    Float lambda_;

    void SetMatrix() {
        auto rot_a = Matrix2D<Float>{};
        auto rot_b = Matrix2D<Float>{};
        auto sq = Matrix2D<Float>{};
        rot_a << std::cos(alpha_), -std::sin(alpha_), std::sin(alpha_), std::cos(alpha_);
        rot_b << std::cos(beta_), -std::sin(beta_), std::sin(beta_), std::cos(beta_);
        sq << std::exp(-lambda_), 0, 0, std::exp(lambda_);
        this->operation_matrix_ = Matrix2D<Float>{rot_a * sq * rot_b};
    }
    static Arbitrary<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                               const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 3) { throw std::invalid_argument("Size of 'params' must be 3"); }
        return Arbitrary<Float>{modes[0], params[0], params[1], params[2]};
    }
};

template <std::floating_point Float>
class BeamSplitter : public BinaryOperation<Float> {
public:
    explicit BeamSplitter(const std::size_t mode0, const std::size_t mode1, const Float sqrt_r,
                          const Float theta_rel)
        : BinaryOperation<Float>(mode0, mode1), sqrt_r_(sqrt_r), theta_rel_(theta_rel) {
        if (sqrt_r < 0 or sqrt_r > 1) {
            throw SimulationError(
                error::InvalidOperation,
                "`sqrt_r` must be in the range [0, 1] in BeamSplitter operation.");
        }
        SetMatrix();
    }

    explicit BeamSplitter(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : BeamSplitter(FromModesAndParams(modes, params)) {}

    Float GetSqrtR() const { return sqrt_r_; }
    Float& GetMutSqrtR() { return sqrt_r_; }
    Float GetThetaRel() const { return theta_rel_; }
    Float& GetMutThetaRel() { return theta_rel_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return sqrt_r_;
            case 1: return theta_rel_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("BeamSplitter({},{})({},{})", sqrt_r_, theta_rel_, this->mode0_,
                           this->mode1_);
    }
    std::vector<Float> Params() const override { return {sqrt_r_, theta_rel_}; }

private:
    Float sqrt_r_;
    Float theta_rel_;

    void SetMatrix() {
        const Float alpha = (theta_rel_ + std::acos(sqrt_r_)) / 2;
        const Float beta = (theta_rel_ - std::acos(sqrt_r_)) / 2;
        const Float sp = std::sin(alpha + beta);
        const Float sm = std::sin(alpha - beta);
        const Float cp = std::cos(alpha + beta);
        const Float cm = std::cos(alpha - beta);
        auto block_diag = Matrix2D<Float>{};
        block_diag << cp * cm, sp * sm, sp * sm, cp * cm;
        auto block_off = Matrix2D<Float>{};
        block_off << -sp * cm, sm * cp, sm * cp, -sp * cm;
        this->operation_matrix_ = MatrixD<Float>{4, 4};
        this->operation_matrix_.block(0, 0, 2, 2) = block_diag;
        this->operation_matrix_.block(2, 2, 2, 2) = block_diag;
        this->operation_matrix_.block(0, 2, 2, 2) = block_off;
        this->operation_matrix_.block(2, 0, 2, 2) = -block_off;
    }
    static BeamSplitter<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                  const std::vector<Float>& params) {
        if (modes.size() != 2) { throw std::invalid_argument("Size of 'modes' must be 2"); }
        if (params.size() != 2) { throw std::invalid_argument("Size of 'params' must be 2"); }
        return BeamSplitter<Float>{modes[0], modes[1], params[0], params[1]};
    }
};

template <std::floating_point Float>
class ControlledZ : public BinaryOperation<Float> {
public:
    explicit ControlledZ(const std::size_t mode0, const std::size_t mode1, const Float g)
        : BinaryOperation<Float>(mode0, mode1), g_(g) {
        SetMatrix();
    }

    explicit ControlledZ(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : ControlledZ(FromModesAndParams(modes, params)) {}

    Float GetG() const { return g_; }
    Float& GetMutG() { return g_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return g_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("ControlledZ({})({},{})", g_, this->mode0_, this->mode1_);
    }
    std::vector<Float> Params() const override { return {g_}; }

private:
    Float g_;

    void SetMatrix() {
        this->operation_matrix_ = MatrixD<Float>{4, 4};
        this->operation_matrix_.setIdentity();
        this->operation_matrix_(2, 1) = g_;
        this->operation_matrix_(3, 0) = g_;
    }
    static ControlledZ<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                 const std::vector<Float>& params) {
        if (modes.size() != 2) { throw std::invalid_argument("Size of 'modes' must be 2"); }
        if (params.size() != 1) { throw std::invalid_argument("Size of 'params' must be 1"); }
        return ControlledZ<Float>{modes[0], modes[1], params[0]};
    }
};

template <std::floating_point Float>
class TwoModeShear : public BinaryOperation<Float> {
public:
    explicit TwoModeShear(const std::size_t mode0, const std::size_t mode1, const Float a,
                          const Float b)
        : BinaryOperation<Float>(mode0, mode1), a_(a), b_(b) {
        SetMatrix();
    }

    explicit TwoModeShear(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : TwoModeShear(FromModesAndParams(modes, params)) {}

    Float GetA() const { return a_; }
    Float& GetMutA() { return a_; }
    Float GetB() const { return b_; }
    Float& GetMutB() { return b_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return a_;
            case 1: return b_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("TwoModeShear({},{})({},{})", a_, b_, this->mode0_, this->mode1_);
    }
    std::vector<Float> Params() const override { return {a_, b_}; }

private:
    Float a_;
    Float b_;

    void SetMatrix() {
        this->operation_matrix_ = MatrixD<Float>{4, 4};
        this->operation_matrix_.setIdentity();
        this->operation_matrix_(2, 0) = 2 * a_;
        this->operation_matrix_(3, 1) = 2 * a_;
        this->operation_matrix_(2, 1) = b_;
        this->operation_matrix_(3, 0) = b_;
    }
    static TwoModeShear<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                  const std::vector<Float>& params) {
        if (modes.size() != 2) { throw std::invalid_argument("Size of 'modes' must be 2"); }
        if (params.size() != 2) { throw std::invalid_argument("Size of 'params' must be 2"); }
        return TwoModeShear<Float>{modes[0], modes[1], params[0], params[1]};
    }
};

template <std::floating_point Float>
class Manual : public BinaryOperation<Float> {
public:
    explicit Manual(const std::size_t mode0, const std::size_t mode1, const Float theta_a,
                    const Float theta_b, const Float theta_c, const Float theta_d)
        : BinaryOperation<Float>(mode0, mode1), theta_a_(theta_a), theta_b_(theta_b),
          theta_c_(theta_c), theta_d_(theta_d) {
        if (IsEquivModPi<Float>(theta_a, theta_b)) {
            throw SimulationError(
                error::InvalidOperation,
                "`theta_a` must not be equal to `theta_b` modulo pi in Manual operation.");
        }
        if (IsEquivModPi<Float>(theta_c, theta_d)) {
            throw SimulationError(
                error::InvalidOperation,
                "`theta_c` must not be equal to `theta_d` modulo pi in Manual operation.");
        }
        SetMatrix();
    }

    explicit Manual(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : Manual(FromModesAndParams(modes, params)) {}

    Float GetThetaA() const { return theta_a_; }
    Float& GetMutThetaA() { return theta_a_; }
    Float GetThetaB() const { return theta_b_; }
    Float& GetMutThetaB() { return theta_b_; }
    Float GetThetaC() const { return theta_c_; }
    Float& GetMutThetaC() { return theta_c_; }
    Float GetThetaD() const { return theta_d_; }
    Float& GetMutThetaD() { return theta_d_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return theta_a_;
            case 1: return theta_b_;
            case 2: return theta_c_;
            case 3: return theta_d_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("Manual({},{},{},{})({},{})", theta_a_, theta_b_, theta_c_, theta_d_,
                           this->mode0_, this->mode1_);
    }
    std::vector<Float> Params() const override { return {theta_a_, theta_b_, theta_c_, theta_d_}; }

private:
    Float theta_a_;
    Float theta_b_;
    Float theta_c_;
    Float theta_d_;

    void SetMatrix() {
        auto block_diag_down = Matrix2D<Float>{};
        block_diag_down << 1, -1, 1, 1;
        auto bs_down = MatrixD<Float>{4, 4};
        bs_down.setZero();
        bs_down.block(0, 0, 2, 2) = block_diag_down;
        bs_down.block(2, 2, 2, 2) = block_diag_down;

        auto block_diag_up = Matrix2D<Float>{};
        block_diag_up << 1, 1, -1, 1;
        auto bs_up = MatrixD<Float>{4, 4};
        bs_up.setZero();
        bs_up.block(0, 0, 2, 2) = block_diag_up;
        bs_up.block(2, 2, 2, 2) = block_diag_up;

        const auto v2_ba = V(theta_b_, theta_a_);
        auto v4_ba = MatrixD<Float>{4, 4};
        v4_ba.setZero();
        v4_ba(1, 1) = 1;
        v4_ba(3, 3) = 1;
        v4_ba(0, 0) = v2_ba(0, 0);
        v4_ba(0, 2) = v2_ba(0, 1);
        v4_ba(2, 0) = v2_ba(1, 0);
        v4_ba(2, 2) = v2_ba(1, 1);

        const auto v2_dc = V(theta_d_, theta_c_);
        auto v4_dc = MatrixD<Float>{4, 4};
        v4_dc.setZero();
        v4_dc(0, 0) = 1;
        v4_dc(2, 2) = 1;
        v4_dc(1, 1) = v2_dc(0, 0);
        v4_dc(1, 3) = v2_dc(0, 1);
        v4_dc(3, 1) = v2_dc(1, 0);
        v4_dc(3, 3) = v2_dc(1, 1);

        this->operation_matrix_ = bs_up * v4_ba * v4_dc * bs_down / 2;
    }

    Matrix2D<Float> V(const Float theta_1, const Float theta_2) const {
        const Float c1 = std::cos(theta_1);
        const Float s1 = std::sin(theta_1);
        const Float c2 = std::cos(theta_2);
        const Float s2 = std::sin(theta_2);
        auto mat1 = Matrix2D<Float>{};
        mat1 << c2, c1, s2, s1;
        auto mat2 = Matrix2D<Float>{};
        mat2 << s1, c1, s2, c2;
        return -mat1 * mat2 / std::sin(theta_2 - theta_1);
    }

    static Manual<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                            const std::vector<Float>& params) {
        if (modes.size() != 2) { throw std::invalid_argument("Size of 'modes' must be 2"); }
        if (params.size() != 4) { throw std::invalid_argument("Size of 'params' must be 4"); }
        return Manual<Float>{modes[0], modes[1], params[0], params[1], params[2], params[3]};
    }
};

template <std::floating_point Float>
class HomodyneSingleModeX : public Operation<Float, Float> {
public:
    explicit HomodyneSingleModeX(const std::size_t mode) : mode_(mode) {}

    /**
     * @brief Construct a new HomodyneSingleModeX object.
     *
     * @param mode Mode to measure.
     * @param selection Desired measured value.
     * @details Normally, a measurement yields random measured values, but when a measurement is
     * performed using a HomodyneSingleModeX object created with 'selection' having its value, the
     * measured value is fixed to the specified value.
     */
    explicit HomodyneSingleModeX(const std::size_t mode, const std::optional<Float>& selection)
        : mode_(mode), selection_(selection) {}

    explicit HomodyneSingleModeX(const std::vector<std::size_t>& modes,
                                 const std::vector<Float>& params)
        : HomodyneSingleModeX(FromModesAndParams(modes, params)) {}

    Float operator()(State<Float>& state) const override {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::HomodyneSingleModeX>{state.GetShot()};
#endif
        if (selection_) { return (*this)(state, selection_.value()); }
        CheckModeIsInState(state);
        const SamplingResult result = Sampling(state);
        PostSelectBySamplingResult(state, result);
        return result.measured_x;
    }

    /**
     * @brief Update the state assuming the measured value is 'val'.
     * @note This method is mainly used for debug purpose.
     */
    Float operator()(State<Float>& state, const Float val) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::HomodyneSingleModeX>{state.GetShot()};
#endif
        CheckModeIsInState(state);
        PostSelectByMeasuredValue(state, val);
        return val;
    }

    const std::optional<Float>& GetSelection() const { return selection_; }
    std::optional<Float>& GetMutSelection() { return selection_; }

    /**
     * @brief Returns a reference to the selection parameter if 0 is specified.
     *
     * @param idx The index of the parameter to retrieve.
     * @return A reference to the selection parameter.
     * @throws std::invalid_argument If the selection parameter is specified but empty, or if a
     * non-existent parameter is specified.
     */
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0:
                if (!selection_) {
                    throw std::invalid_argument(
                        "The input operation index refers to the selection parameter, but it is "
                        "empty.");
                }
                return selection_.value();
            default: break;
        }
        return Operation<Float, Float>::GetMutParam(idx);
    }

    std::string ToString() const override {
        if (selection_) {
            return fmt::format("HomodyneSingleModeX({})({})", selection_.value(), mode_);
        }
        return fmt::format("HomodyneSingleModeX()({})", mode_);
    }

    std::vector<std::size_t> Modes() const override { return {mode_}; }
    std::vector<Float> Params() const override {
        if (selection_) { return {selection_.value()}; }
        return {};
    }

    void CheckModeIsInState([[maybe_unused]] const State<Float>& state) const {
#ifndef NDEBUG
        if (this->mode_ >= state.NumModes()) {
            throw std::logic_error(
                "Measured mode indices must be lower than the number of modes in the state");
        }
#endif
    }

    struct SamplingResult {
        Float measured_x;                                     //!< measured value.
        std::vector<std::complex<Float>> original_peak_vals;  //!< PDF value of each peak.
        Float original_pdf_val;                               //!< sum of original_peak_vals.
    };
    SamplingResult Sampling(const State<Float>& state,
                            const std::optional<Float>& sampled_x = std::nullopt) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::Sampling>{state.GetShot()};
#endif
        if (sampled_x) {
            auto result = SamplingResult{};
            result.original_peak_vals.resize(state.NumPeaks());
            result.measured_x = sampled_x.value();
            result.original_pdf_val =
                CalcOriginalPdf(state, sampled_x.value(), result.original_peak_vals);
            return result;
        }

        constexpr std::int32_t MaxAttemptsOfRejectionSampling = 10'000;

        const auto num_peaks = state.NumPeaks();
        const auto x_idx = static_cast<long>(this->mode_);

        // Calculate types of peaks.
        const auto peak_types = CalcPeakTypes(state);

        // Calculate coeffs of proposal distribution.
        auto proposal_peak_coeffs = std::vector<Float>(num_peaks, 0);
        for (std::size_t m = 0; m < num_peaks; ++m) {
            const auto peak_type = peak_types[m];
            const auto& mean = state.GetMean(m);
            const auto& cov = state.GetCov(m);

            if (IsCoeffNeg(peak_type) && IsMeanReal(peak_type)) {
                proposal_peak_coeffs[m] = 0;
            } else if (IsMeanReal(peak_type)) {
                proposal_peak_coeffs[m] = std::abs(state.GetCoeff(m));
            } else {
                // Mean is complex.
                const Float exponent =
                    mean(x_idx).imag() * mean(x_idx).imag() / (2 * cov(x_idx, x_idx));
                proposal_peak_coeffs[m] = std::abs(state.GetCoeff(m)) * std::exp(exponent);
            }

            if (std::isinf(proposal_peak_coeffs[m])) {
                throw SimulationError(
                    error::Overflow,
                    fmt::format("Proposal coefficient of peak {} is infinite.", m));
            }
            if (std::isnan(proposal_peak_coeffs[m])) {
                throw SimulationError(
                    error::NaN,
                    fmt::format("Proposal coefficient of peak {} is NaN. Maybe overflow occurred.",
                                m));
            }
        }

        auto peak_selector = std::discrete_distribution<std::size_t>{proposal_peak_coeffs.begin(),
                                                                     proposal_peak_coeffs.end()};

        auto ret = SamplingResult{};
        ret.original_peak_vals.resize(num_peaks);
        for (std::int32_t i_attempt = 1; i_attempt <= MaxAttemptsOfRejectionSampling; ++i_attempt) {
            // Peak sampling
            const std::size_t sampled_peak_idx = peak_selector(state.GetEngine());

            // Gaussian sampling
            const auto pdf_mean = state.GetMean(sampled_peak_idx)(x_idx).real();
            const auto pdf_var = state.GetCov(sampled_peak_idx)(x_idx, x_idx);
            const auto pdf_cov = std::sqrt(pdf_var);

            if (std::isinf(pdf_mean)) {
                throw SimulationError(
                    error::Overflow, fmt::format("Mean of peak {} is infinite.", sampled_peak_idx));
            }
            if (std::isnan(pdf_mean)) {
                throw SimulationError(
                    error::NaN, fmt::format("Mean of peak {} is NaN. Maybe overflow occurred.",
                                            sampled_peak_idx));
            }
            if (std::isinf(pdf_var)) {
                throw SimulationError(
                    error::Overflow,
                    fmt::format("Covariance of peak {} is infinite.", sampled_peak_idx));
            }
            if (std::isnan(pdf_var)) {
                throw SimulationError(
                    error::NaN,
                    fmt::format("Covariance of peak {} is NaN. Maybe overflow occurred.",
                                sampled_peak_idx));
            }
            auto sampled_pdf = std::normal_distribution<Float>{pdf_mean, pdf_cov};
            const Float sampled_x = sampled_pdf(state.GetEngine());

            // Uniform sampling
            const Float proposal_val =
                CalcProposalValue(state, peak_types, proposal_peak_coeffs, sampled_x);
            auto uniform_pdf = std::uniform_real_distribution<Float>{0, proposal_val};
            const Float y = uniform_pdf(state.GetEngine());

            // Accept 'sampled_x' if 'y' is smaller than or equal to the original PDF value.
            const Float original_pdf_val =
                CalcOriginalPdf(state, sampled_x, ret.original_peak_vals);
            CheckProposalValueIsGreaterThanOriginalPdfValue(proposal_val, original_pdf_val);
            if (y <= original_pdf_val) {
                LOG_DEBUG(
                    "The rejection sampling method gets a sampling value on the {}-th attempt.",
                    i_attempt);
                ret.measured_x = sampled_x;
                ret.original_pdf_val = original_pdf_val;
                return ret;
            }
        }

        throw SimulationError(
            error::Unknown,
            fmt::format(
                "Rejection sampling failed after {} attempts. No valid samples were accepted.",
                MaxAttemptsOfRejectionSampling));
    }

    /**
     * @brief Update 'peak' assuming the 'x_idx' homodyne measurement get the 'result'.
     *
     * @param state bosonic state.
     * @param result sampling result of 'x_idx' homodyne measurement.
     * @param peak peak index.
     */
    void PostSelectBySamplingResult(State<Float>& state, const SamplingResult& result,
                                    const std::size_t peak) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::PostSelectBySamplingResultPerPeak>{
            state.GetShot(), peak};
#endif
        const auto num_modes = static_cast<long>(state.NumModes());
        const auto x_idx = static_cast<long>(this->mode_);
        const auto p_idx = static_cast<long>(this->mode_ + state.NumModes());

        auto& mean = state.GetMutMean(peak);
        auto& cov = state.GetMutCov(peak);

        // Update mean_A and cov_A.
        const auto cov_copy = cov.col(x_idx);
        for (long i = 0; i < 2 * num_modes; ++i) {
            if (i == x_idx || i == p_idx) { continue; }
            const auto tmp = (result.measured_x - mean(x_idx)) / cov(x_idx, x_idx);
            mean(i) += tmp * cov_copy(i);
            for (long j = i; j < 2 * num_modes; ++j) {
                if (j == x_idx || j == p_idx) { continue; }

                cov(j, i) = cov(i, j) -= cov_copy(i) * cov_copy(j) / cov_copy(x_idx);
            }
        }

        // Update peak_coeffs, mean_B, cov_AB, and cov_B.
        state.GetMutCoeff(peak) = result.original_peak_vals[peak] / result.original_pdf_val;
        mean(x_idx) = mean(p_idx) = 0;
        for (long i = 0; i < 2 * num_modes; ++i) {
            cov(x_idx, i) = cov(i, x_idx) = 0;
            cov(p_idx, i) = cov(i, p_idx) = 0;
        }
        cov(x_idx, x_idx) = cov(p_idx, p_idx) = Hbar<Float> / 2;
    }

    /**
     * @brief Update bosonic state assuming the 'x_idx' homodyne measurement get the 'result'.
     *
     * @param state bosonic state.
     * @param result sampling result of 'x_idx' homodyne measurement.
     */
    void PostSelectBySamplingResult(State<Float>& state, const SamplingResult& result) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile =
            profiler::Profiler<profiler::Section::PostSelectBySamplingResult>{state.GetShot()};
#endif
        const auto num_peaks = state.NumPeaks();
        for (std::size_t m = 0; m < num_peaks; ++m) {
            PostSelectBySamplingResult(state, result, m);
        }
    }

private:
    std::size_t mode_;
    std::optional<Float> selection_;

    static constexpr void CheckProposalValueIsGreaterThanOriginalPdfValue(
        [[maybe_unused]] Float proposal_val, [[maybe_unused]] Float original_pdf_val) {
#ifndef NDEBUG
        // Add a small value (Eps) to account for potential numerical errors in floating-point
        // calculations, as the proposed value may be estimated as smaller than the actual value
        // due to these errors.
        if (!(original_pdf_val < proposal_val + Eps<Float>)) {
            throw std::logic_error(
                fmt::format("The proposal value {} does not encompass the original PDF value {}.",
                            proposal_val, original_pdf_val));
        }
#endif
    }

    // clang-format off
    static constexpr std::uint8_t CoeffReal = 0b00'000'001u;
    static constexpr std::uint8_t CoeffNeg  = 0b00'000'010u;
    static constexpr std::uint8_t MeanReal  = 0b00'000'100u;
    // clang-format on

    static constexpr auto IsCoeffReal(std::uint8_t peak_type) {
        return 0 != (peak_type & CoeffReal);
    }
    static constexpr auto IsCoeffNeg(std::uint8_t peak_type) { return 0 != (peak_type & CoeffNeg); }
    static constexpr auto IsMeanReal(std::uint8_t peak_type) { return 0 != (peak_type & MeanReal); }

    /**
     * @brief Calculate types of peaks.
     * @details The peak type is a bit field with the following structure:
     *
     * * If the coefficient of a peak is real, CoeffReal is set.
     * * If the coefficient of a peak is negative, CoeffNeg is set.
     * * If the mean of a peak is real, MeanReal is set.
     *
     * @param state
     * @return std::vector<std::uint8_t> peak types (size is the number of peaks in 'state').
     */
    std::vector<std::uint8_t> CalcPeakTypes(const State<Float>& state) const {
        const auto num_peaks = state.NumPeaks();
        const auto x_idx = static_cast<long>(this->mode_);
        const auto p_idx = static_cast<long>(this->mode_ + state.NumModes());

        auto ret = std::vector<std::uint8_t>(num_peaks, 0);
        for (std::size_t m = 0; m < num_peaks; ++m) {
            const std::complex<Float>& coeff = state.GetCoeff(m);
            const std::complex<Float>& mean_x = state.GetMean(m)(x_idx);
            const std::complex<Float>& mean_p = state.GetMean(m)(p_idx);

            if (IsReal(coeff)) {
                ret[m] |= CoeffReal;
                if (coeff.real() < 0) { ret[m] |= CoeffNeg; }
            }
            if (IsReal(mean_x) && IsReal(mean_p)) { ret[m] |= MeanReal; }
        }
        return ret;
    }

    /**
     * @brief Calculate the value of proposal distribution.
     *
     * @param state
     * @param peak_types types of each peak.
     * @param proposal_peak_coeffs coefficients of each peak of proposal distribution.
     * @param x x value.
     * @return Float the value oof proposal distribution at 'x'.
     */
    Float CalcProposalValue(const State<Float>& state, const std::vector<std::uint8_t>& peak_types,
                            const std::vector<Float>& proposal_peak_coeffs, Float x) const {
        const auto num_peaks = state.NumPeaks();
        const auto x_idx = static_cast<long>(this->mode_);

        Float proposal_val = 0;
        for (std::size_t m = 0; m < num_peaks; ++m) {
            const auto peak_type = peak_types[m];
            const auto& mean = state.GetMean(m);
            const auto& cov = state.GetCov(m);

            if (IsCoeffNeg(peak_type) && IsMeanReal(peak_type)) { continue; }

            const auto pdf = boost::math::normal_distribution<Float>(mean(x_idx).real(),
                                                                     std::sqrt(cov(x_idx, x_idx)));
            proposal_val += proposal_peak_coeffs[m] * boost::math::pdf(pdf, x);
        }
        return proposal_val;
    }

    /**
     * @brief Calculate original PDF values.
     *
     * @param state bosonic state.
     * @param x x value.
     * @param o_original_peak_vals PDF value at 'x' of each peak (vector size must be equal the
     * number of peaks in 'state').
     * @return Float sum of 'o_original_peak_vals'.
     */
    Float CalcOriginalPdf(const State<Float>& state, Float x,
                          std::vector<std::complex<Float>>& o_original_peak_vals) const {
        const auto num_peaks = state.NumPeaks();
        const auto x_idx = static_cast<long>(mode_);

#ifndef NDEBUG
        if (o_original_peak_vals.size() != num_peaks) {
            throw std::logic_error(
                "Size of o_original_peak_vals is not equal to the number of peaks in 'state'.");
        }
#endif

        std::complex<Float> original_pdf_val = 0;
        for (std::size_t m = 0; m < num_peaks; ++m) {
            const auto& coeff = state.GetCoeff(m);
            const auto& mean = state.GetMean(m);
            const auto& cov = state.GetCov(m);

            original_pdf_val += o_original_peak_vals[m] =
                coeff * General1DGaussianPdf<Float>(x, mean(x_idx), std::sqrt(cov(x_idx, x_idx)));
        }
        if (!IsReal(original_pdf_val)) {
            throw SimulationError(error::Unknown,
                                  "Value of probability density function has a significant "
                                  "imaginary component. This may indicate an accumulation of "
                                  "floating-point errors from preceding calculations.");
        }

        return original_pdf_val.real();
    }

    /**
     * @brief Update bosonic state assuming the x_idx homodyne measurement get the 'sampled_x'
     * value.
     *
     * @param state bosonic state.
     * @param sampled_x measured value of x_idx homodyne measurement.
     */
    void PostSelectByMeasuredValue(State<Float>& state, const Float sampled_x) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile =
            profiler::Profiler<profiler::Section::PostSelectByMeasuredValue>{state.GetShot()};
#endif
        PostSelectBySamplingResult(state, Sampling(state, sampled_x));
    }

    static HomodyneSingleModeX<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                         const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() == 0) {
            return HomodyneSingleModeX<Float>{modes[0]};
        } else if (params.size() == 1) {
            return HomodyneSingleModeX<Float>{modes[0], params[0]};
        }
        throw std::invalid_argument("The size of 'params' must be either 0 or 1");
    }
};

template <std::floating_point Float>
class HomodyneSingleMode : public Operation<Float, Float> {
public:
    explicit HomodyneSingleMode(const std::size_t mode, const Float theta)
        : mode_(mode), theta_(theta) {}

    /**
     * @brief Construct a new HomodyneSingleMode object.
     *
     * @param mode Mode to measure.
     * @param theta Measurement angle.
     * @param selection Desired measured value.
     * @details Normally, a measurement yields random measured values, but when a measurement is
     * performed using a HomodyneSingleModeX object created with this constructor, the measured
     * value is fixed to the specified value 'selection'.
     */
    explicit HomodyneSingleMode(const std::size_t mode, const Float theta, const Float selection)
        : mode_(mode), theta_(theta), selection_(selection) {}

    explicit HomodyneSingleMode(const std::vector<std::size_t>& modes,
                                const std::vector<Float>& params)
        : HomodyneSingleMode(FromModesAndParams(modes, params)) {}

    Float operator()(State<Float>& state) const override {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::HomodyneSingleMode>{state.GetShot()};
#endif
        PhaseRotation<Float>{mode_, -std::numbers::pi_v<Float> / 2 + theta_}(state);
        return HomodyneSingleModeX<Float>{mode_, selection_}(state);
    }

    /**
     * @brief Update the state assuming the measured value is 'val'.
     * @note This method is mainly used for debug purpose.
     */
    Float operator()(State<Float>& state, const Float val) const {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::HomodyneSingleMode>{state.GetShot()};
#endif
        PhaseRotation<Float>{mode_, -std::numbers::pi_v<Float> / 2 + theta_}(state);
        return HomodyneSingleModeX<Float>{mode_}(state, val);
    }

    Float GetTheta() const { return theta_; }
    Float& GetMutTheta() { return theta_; }
    const std::optional<Float>& GetSelection() const { return selection_; }
    std::optional<Float>& GetMutSelection() { return selection_; }

    /**
     * @brief Returns a reference to the parameter at the specified index: theta (index 0),
     * selection parameter (index 1).
     * @return A reference to the parameter at the specified index.
     * @throws std::invalid_argument If the selection parameter is specified but empty, or if a
     * non-existent parameter is specified.
     */
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return theta_;
            case 1:
                if (!selection_) {
                    throw std::invalid_argument(
                        "The input operation index refers to the selection parameter, but it is "
                        "empty.");
                }
                return selection_.value();
            default: break;
        }
        return Operation<Float, Float>::GetMutParam(idx);
    }

    std::string ToString() const override {
        if (selection_) {
            return fmt::format("HomodyneSingleMode({},{})({})", theta_, selection_.value(), mode_);
        }
        return fmt::format("HomodyneSingleMode({})({})", theta_, mode_);
    }

    std::vector<std::size_t> Modes() const override { return {mode_}; }
    std::vector<Float> Params() const override {
        if (selection_) { return {theta_, selection_.value()}; }
        return {theta_};
    }

    void CheckModeIsInState([[maybe_unused]] const State<Float>& state) const {
#ifndef NDEBUG
        if (this->mode_ >= state.NumModes()) {
            throw std::logic_error(
                "Measured mode indices must be lower than the number of modes in the state");
        }
#endif
    }

private:
    std::size_t mode_;
    Float theta_;
    std::optional<Float> selection_;

    static HomodyneSingleMode<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                        const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() == 1) {
            return HomodyneSingleMode<Float>{modes[0], params[0]};
        } else if (params.size() == 2) {
            return HomodyneSingleMode<Float>{modes[0], params[0], params[1]};
        } else {
            throw std::invalid_argument("The size of 'params' must be either 1 or 2.");
        }
    }
};

namespace util {
template <std::floating_point Float>
class BeamSplitter50 : public BinaryOperation<Float> {
public:
    explicit BeamSplitter50(const std::size_t mode0, const std::size_t mode1)
        : BinaryOperation<Float>(mode0, mode1) {
        SetMatrix();
    }

    explicit BeamSplitter50(const std::vector<std::size_t>& modes, const std::vector<Float>& params)
        : BeamSplitter50(FromModesAndParams(modes, params)) {}

    std::string ToString() const override {
        return fmt::format("BeamSplitter50()({},{})", this->mode0_, this->mode1_);
    }

private:
    void SetMatrix() {
        auto block_diag = Matrix2D<Float>{};
        block_diag << 1, -1, 1, 1;
        block_diag /= static_cast<Float>(std::sqrt(2));
        this->operation_matrix_ = MatrixD<Float>{4, 4};
        this->operation_matrix_.setZero();
        this->operation_matrix_.block(0, 0, 2, 2) = block_diag;
        this->operation_matrix_.block(2, 2, 2, 2) = block_diag;
    }
    static BeamSplitter50<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                    const std::vector<Float>& params) {
        if (modes.size() != 2) { throw std::invalid_argument("Size of 'modes' must be 2"); }
        if (params.size() != 0) { throw std::invalid_argument("Size of 'params' must be 0"); }
        return BeamSplitter50<Float>{modes[0], modes[1]};
    }
};

template <std::floating_point Float>
class BeamSplitterStd : public BinaryOperation<Float> {
public:
    explicit BeamSplitterStd(const std::size_t mode0, const std::size_t mode1, const Float theta,
                             const Float phi)
        : BinaryOperation<Float>(mode0, mode1), theta_(theta), phi_(phi) {
        SetMatrix();
    }

    explicit BeamSplitterStd(const std::vector<std::size_t>& modes,
                             const std::vector<Float>& params)
        : BeamSplitterStd(FromModesAndParams(modes, params)) {}

    Float GetTheta() const { return theta_; }
    Float& GetMutTheta() { return theta_; }
    Float GetPhi() const { return phi_; }
    Float& GetMutPhi() { return phi_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return theta_;
            case 1: return phi_;
            default: break;
        }
        return Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("BeamSplitterStd({},{})({},{})", theta_, phi_, this->mode0_,
                           this->mode1_);
    }
    std::vector<Float> Params() const override { return {theta_, phi_}; }

private:
    Float theta_;
    Float phi_;

    void SetMatrix() {
        const Float st = std::sin(theta_);
        const Float sp = std::sin(phi_);
        const Float ct = std::cos(theta_);
        const Float cp = std::cos(phi_);
        auto block_diag = Matrix2D<Float>{};
        block_diag << ct, -st * cp, st * cp, ct;
        auto block_off = Matrix2D<Float>{};
        block_off << 0, -st * sp, -st * sp, 0;
        this->operation_matrix_ = MatrixD<Float>{4, 4};
        this->operation_matrix_.block(0, 0, 2, 2) = block_diag;
        this->operation_matrix_.block(2, 2, 2, 2) = block_diag;
        this->operation_matrix_.block(0, 2, 2, 2) = block_off;
        this->operation_matrix_.block(2, 0, 2, 2) = -block_off;
    }
    static BeamSplitterStd<Float> FromModesAndParams(const std::vector<std::size_t>& modes,
                                                     const std::vector<Float>& params) {
        if (modes.size() != 2) { throw std::invalid_argument("Size of 'modes' must be 2"); }
        if (params.size() != 2) { throw std::invalid_argument("Size of 'params' must be 2"); }
        return BeamSplitterStd<Float>{modes[0], modes[1], params[0], params[1]};
    }
};

/**
 * @brief Operation to replace a mode with a squeezed state.
 *
 * @tparam Float
 * @details It is assumed that the target mode is not entangled with any other mode.
 */
template <std::floating_point Float>
class ReplaceBySqueezedState : public operation::Operation<Float, void> {
public:
    /**
     * @brief Construct a new ReplaceBySqueezedState object.
     *
     * @param mode Index of the mode replaced by squeezing.
     * @param squeezing_level Squeezing level in dB.
     */
    explicit ReplaceBySqueezedState(const std::size_t mode, const Float squeezing_level,
                                    const Float phi = 0)
        : mode_(mode), squeezing_level_(squeezing_level), phi_(phi) {}

    explicit ReplaceBySqueezedState(const std::vector<std::size_t>& modes,
                                    const std::vector<Float>& params) {
        if (modes.size() != 1) { throw std::invalid_argument("Size of 'modes' must be 1"); }
        if (params.size() != 2) { throw std::invalid_argument("Size of 'params' must be 2"); }
        mode_ = modes[0];
        squeezing_level_ = params[0];
        phi_ = params[1];
    }

    void operator()(State<Float>& state) const override {
        this->CheckModeIsInState(state);
        this->UpdateState(state);
    }
    void operator()(State<Float>& state, const std::size_t peak_idx) const {
        this->CheckModeIsInState(state);
        this->UpdatePeak(state, peak_idx);
    }
    Float GetSqueezingLevel() const { return squeezing_level_; }
    Float& GetMutSqueezingLevel() { return squeezing_level_; }
    Float GetPhi() const { return phi_; }
    Float& GetMutPhi() { return phi_; }
    Float& GetMutParam(const std::size_t idx) override {
        switch (idx) {
            case 0: return squeezing_level_;
            case 1: return phi_;
            default: break;
        }
        return operation::Operation<Float, void>::GetMutParam(idx);
    }

    std::string ToString() const override {
        return fmt::format("ReplaceBySqueezedState({},{})({})", squeezing_level_, phi_, mode_);
    }

    std::vector<std::size_t> Modes() const override { return {mode_}; }
    std::vector<Float> Params() const override { return {squeezing_level_, phi_}; }

private:
    std::size_t mode_;
    Float squeezing_level_;
    Float phi_;

    void CheckModeIsInState([[maybe_unused]] const State<Float>& state) const {
#ifndef NDEBUG
        if (this->mode_ >= state.NumModes()) {
            throw std::logic_error(
                "Measured mode indices must be lower than the "
                "number of modes in the state");
        }
#endif
    }

    void UpdatePeak(State<Float>& state, const std::size_t peak_idx) const {
        const auto operated_index_x = static_cast<std::int64_t>(this->mode_);
        const auto operated_index_p = static_cast<std::int64_t>(this->mode_ + state.NumModes());
        const auto operated_indices = std::vector<std::int64_t>{operated_index_x, operated_index_p};
        auto& mu = state.GetMutMean(peak_idx);
        auto& sigma = state.GetMutCov(peak_idx);

        mu(operated_indices).setZero();
        sigma(operated_indices, operated_indices) = GenerateSqueezedCov(squeezing_level_, phi_);

#ifndef NDEBUG
        // Check that the off-diagonal elements are almost zero
        for (std::size_t i = 0; i < operated_indices.size(); ++i) {
            for (std::int64_t j = 0; static_cast<std::size_t>(j) < 2 * state.NumModes(); ++j) {
                if ((std::abs(sigma(operated_indices[i], j)) > 1e-5 ||
                     std::abs(sigma(j, operated_indices[i])) > 1e-5) &&
                    std::find(operated_indices.begin(), operated_indices.end(), j) ==
                        operated_indices.end()) {
                    throw std::runtime_error(fmt::format("sigma({},{}) is not close to zero: {}",
                                                         operated_indices[i], j,
                                                         sigma(operated_indices[i], j)));
                }
            }
        }
#endif
    }

    void UpdateState(State<Float>& state) const {
        for (std::size_t m = 0; m < state.NumPeaks(); ++m) { UpdatePeak(state, m); }
    }
};
}  // namespace util
}  // namespace bosim::operation
