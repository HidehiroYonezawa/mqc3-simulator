#pragma once

#include <array>
#include <vector>

#include "bosim/circuit.h"
#include "bosim/operation.h"
#include "bosim/python/function.h"

#include "mqc3_cloud/program/v1/machinery.pb.h"

namespace bosim::machinery {
using PbMachineryRepresentation = mqc3_cloud::program::v1::MachineryRepresentation;
using PbMacronodeAngle = PbMachineryRepresentation::MacronodeAngle;
using PbDisplacementComplex = PbMachineryRepresentation::DisplacementComplex;
using PbFeedForwardCoefficientGenerationMethod =
    mqc3_cloud::program::v1::FeedForwardCoefficientGenerationMethod;
using PbMachineryFF = mqc3_cloud::program::v1::MachineryFF;
using PbPythonFunction = ::mqc3_cloud::common::v1::PythonFunction;

template <std::floating_point Float>
struct MacronodeAngle {
    Float theta_a;
    Float theta_b;
    Float theta_c;
    Float theta_d;

    MacronodeAngle() = default;
    MacronodeAngle(const Float theta_a, const Float theta_b, const Float theta_c,
                   const Float theta_d)
        : theta_a(theta_a), theta_b(theta_b), theta_c(theta_c), theta_d(theta_d) {
#ifndef NDEBUG
        const auto ab = IsEquivModPi(theta_a, theta_b);
        const auto bc = IsEquivModPi(theta_b, theta_c);
        const auto cd = IsEquivModPi(theta_c, theta_d);
        if (!(ab && bc && cd) && (ab || cd)) {
            throw std::invalid_argument(
                "The four angles must satisfy the following relation modulo pi.\n"
                "`theta_a` == `theta_b` == `theta_c` == `theta_d` or\n"
                "(`theta_a` != `theta_b` and `theta_c` != `theta_d`)");
        }
#endif
    }
    explicit MacronodeAngle(const PbMacronodeAngle& pb)
        : MacronodeAngle(pb.theta_a(), pb.theta_b(), pb.theta_c(), pb.theta_d()) {}

    Float operator[](size_t i_theta) const {
        switch (i_theta) {
            case 0: return theta_a;
            case 1: return theta_b;
            case 2: return theta_c;
            case 3: return theta_d;
            default: throw std::runtime_error("Invalid i_theta.");
        }
    }
    Float& operator[](size_t i_theta) {
        switch (i_theta) {
            case 0: return theta_a;
            case 1: return theta_b;
            case 2: return theta_c;
            case 3: return theta_d;
            default: throw std::runtime_error("Invalid i_theta.");
        }
    }

    bool IsMeasurable() const {
        return IsEquivModPi(theta_a, theta_b) && IsEquivModPi(theta_a, theta_c) &&
               IsEquivModPi(theta_a, theta_d) && IsEquivModPi(theta_b, theta_c) &&
               IsEquivModPi(theta_c, theta_d);
    }
};

template <std::floating_point Float>
struct DisplacementComplex {
    Float x;
    Float p;

    DisplacementComplex() = default;
    DisplacementComplex(const Float x, const Float p) : x(x), p(p) {}
    explicit DisplacementComplex(const PbDisplacementComplex& pb)
        : DisplacementComplex(pb.x(), pb.p()) {}
};

struct MachineryFF {
    std::size_t function;
    std::size_t from_macronode;
    std::size_t from_abcd;
    std::size_t to_macronode;
    std::size_t to_parameter;

    MachineryFF() = default;
    explicit MachineryFF(const PbMachineryFF& pb)
        : function(pb.function()), from_macronode(pb.from_macronode()), from_abcd(pb.from_abcd()),
          to_macronode(pb.to_macronode()), to_parameter(pb.to_parameter()) {}
};

template <std::floating_point Float>
struct PhysicalCircuitConfiguration {
    std::size_t n_local_macronodes;
    std::size_t n_steps;
    std::vector<MacronodeAngle<Float>> homodyne_angles;
    std::vector<PbFeedForwardCoefficientGenerationMethod> generating_method_for_ff_coeff_k_plus_1;
    std::vector<PbFeedForwardCoefficientGenerationMethod> generating_method_for_ff_coeff_k_plus_n;
    std::vector<DisplacementComplex<Float>> displacements_k_minus_1;
    std::vector<DisplacementComplex<Float>> displacements_k_minus_n;
    std::vector<std::size_t> readout_macronodes_indices;
    std::vector<MachineryFF> nlffs;
    std::vector<python::PythonFeedForward> functions;

    PhysicalCircuitConfiguration() = default;
    explicit PhysicalCircuitConfiguration(const PbMachineryRepresentation& pb)
        : n_local_macronodes(pb.n_local_macronodes()), n_steps(pb.n_steps()) {
        const auto n_macronodes = static_cast<std::size_t>(pb.homodyne_angles().size());
        homodyne_angles.resize(n_macronodes);
        generating_method_for_ff_coeff_k_plus_1.resize(n_macronodes);
        generating_method_for_ff_coeff_k_plus_n.resize(n_macronodes);
        displacements_k_minus_1.resize(n_macronodes);
        displacements_k_minus_n.resize(n_macronodes);
        for (std::size_t k = 0; k < n_macronodes; ++k) {
            const auto k_int = static_cast<std::int32_t>(k);
            homodyne_angles[k] = MacronodeAngle<Float>{pb.homodyne_angles()[k_int]};
            generating_method_for_ff_coeff_k_plus_1[k] =
                static_cast<PbFeedForwardCoefficientGenerationMethod>(
                    pb.generating_method_for_ff_coeff_k_plus_1()[k_int]);
            generating_method_for_ff_coeff_k_plus_n[k] =
                static_cast<PbFeedForwardCoefficientGenerationMethod>(
                    pb.generating_method_for_ff_coeff_k_plus_n()[k_int]);
            const auto d1 = pb.displacements_k_minus_1()[static_cast<std::int32_t>(k)];
            const auto dn = pb.displacements_k_minus_n()[static_cast<std::int32_t>(k)];
            displacements_k_minus_1[k] = DisplacementComplex<Float>{d1.x(), d1.p()};
            displacements_k_minus_n[k] = DisplacementComplex<Float>{dn.x(), dn.p()};
        }

        readout_macronodes_indices = std::vector<std::size_t>(
            pb.readout_macronodes_indices().begin(), pb.readout_macronodes_indices().end());

        nlffs.reserve(static_cast<std::size_t>(pb.nlffs().size()));
        for (const auto& pb_machinery_ff : pb.nlffs()) { nlffs.emplace_back(pb_machinery_ff); }

        const auto n_functions = pb.functions().size();
        functions.reserve(static_cast<std::size_t>(n_functions));
        for (std::int32_t j = 0; j < n_functions; ++j) {
            const auto codes = std::vector<std::string>(pb.functions()[j].code().begin(),
                                                        pb.functions()[j].code().end());
            functions.emplace_back(codes);
        }
    }
};

template <std::floating_point Float>
struct MacronodeMeasuredValue {
    Float m_a;
    Float m_b;
    Float m_c;
    Float m_d;
};

/**
 * @brief Compute feedforward within an ordinary macronode circuit.
 *
 * @param macro_angle Homodyne p-x angles at the macronode of interest.
 * @param macro_measured_val Measured values at the macronode of interest.
 *
 * @return A pair of DisplacementComplex representing ((X1, P1), (XN, PN)).
 *
 * @throws std::runtime_error Feedforward diverges.
 * @details This function computes (f_mac^1, f_mac^N) = ((X1, P1), (XN, PN)).
 */
template <std::floating_point Float>
auto FMac(const MacronodeAngle<Float>& macro_angle,
          const MacronodeMeasuredValue<Float>& macro_measured_val)
    -> std::pair<DisplacementComplex<Float>, DisplacementComplex<Float>> {
#ifndef NDEBUG
    if (macro_angle.IsMeasurable()) {
        throw std::invalid_argument("Cannot perform FMac on measurable macronode.");
    }
#endif
    auto mat1 = Matrix2D<Float>{};
    mat1 << std::cos(macro_angle.theta_b), std::cos(macro_angle.theta_a),
        std::sin(macro_angle.theta_b), std::sin(macro_angle.theta_a);
    auto mat2 = Matrix2D<Float>{};
    mat2 << std::cos(macro_angle.theta_d), std::cos(macro_angle.theta_c),
        std::sin(macro_angle.theta_d), std::sin(macro_angle.theta_c);
    auto vec1 = Vector2D<Float>{macro_measured_val.m_a, macro_measured_val.m_b};
    auto vec2 = Vector2D<Float>{macro_measured_val.m_c, macro_measured_val.m_d};
    auto term1 = mat1 * vec1 / std::sin(macro_angle.theta_a - macro_angle.theta_b);
    auto term2 = mat2 * vec2 / std::sin(macro_angle.theta_c - macro_angle.theta_d);
    return {DisplacementComplex<Float>{-(term1(0) + term2(0)), -(term1(1) + term2(1))},
            DisplacementComplex<Float>{-(-term1(0) + term2(0)), -(-term1(1) + term2(1))}};
}

/**
 * @brief Compute feedforward within an initialization/measurement macronode circuit.
 *
 * @param macro_angle Homodyne p-x angles at the macronode of interest.
 * @param macro_measured_val Measured values at the macronode of interest.
 *
 * @return A tuple of DisplacementComplex representing ((X1, P1), (XN, PN)).
 *
 * @throws std::runtime_error The macronode is not for initialization.
 * @details This function computes (i_mac^1, i_mac^N) = ((X1, P1), (XN, PN)).
 */
template <std::floating_point Float>
auto IMac(const MacronodeAngle<Float>& macro_angle,
          const MacronodeMeasuredValue<Float>& macro_measured_val)
    -> std::pair<DisplacementComplex<Float>, DisplacementComplex<Float>> {
#ifndef NDEBUG
    if (!macro_angle.IsMeasurable()) {
        throw std::invalid_argument("Cannot perform IMac on non-measurable macronode.");
    }
#endif
    const auto lma = (macro_measured_val.m_a - macro_measured_val.m_b + macro_measured_val.m_c -
                      macro_measured_val.m_d) /
                     2.0;
    const auto lmc = (-macro_measured_val.m_a + macro_measured_val.m_b + macro_measured_val.m_c -
                      macro_measured_val.m_d) /
                     2.0;
    return {
        DisplacementComplex<Float>{-lma * sin(macro_angle.theta_a), lma * cos(macro_angle.theta_a)},
        DisplacementComplex<Float>{-lmc * sin(macro_angle.theta_c),
                                   lmc * cos(macro_angle.theta_c)}};
}

/**
 * @brief Compute feedforward within a macronode circuit.
 *
 * @tparam Float
 * @param macro_angle Homodyne p-x angles at the macronode of interest.
 * @param macro_measured_val Measured values at the macronode of interest.
 * @return std::pair<DisplacementComplex<Float>, DisplacementComplex<Float>> A tuple of
 * DisplacementComplex representing ((X1, P1), (XN, PN)).
 */
template <std::floating_point Float>
auto FIMac(const MacronodeAngle<Float>& macro_angle,
           const MacronodeMeasuredValue<Float>& macro_measured_val)
    -> std::pair<DisplacementComplex<Float>, DisplacementComplex<Float>> {
    if (macro_angle.IsMeasurable()) {
        return IMac(macro_angle, macro_measured_val);
    } else {
        return FMac(macro_angle, macro_measured_val);
    }
}

constexpr std::size_t NOpsPerMacronode = 18;
enum class MacronodeOpType : std::uint8_t {
    DisplacementB = 0,
    DisplacementD,
    BeamsplitterBkDk,
    BeamsplitterAkBk1,
    BeamsplitterCkDkn,
    BeamsplitterBkAk,
    BeamsplitterDkCk,
    HomodyneA,
    HomodyneB,
    HomodyneC,
    HomodyneD,
    BeamsplitterDknBk1,
    LinearFFB,
    LinearFFD
};

/**
 * @brief Get the operation index in the Circuit object.
 *
 * @param k Macronode index.
 * @param op_type Operation type in the macronode k.
 * @return std::size_t Operation index in the Circuit object.
 */
inline std::size_t MacronodeOpIdx(const std::size_t k, const MacronodeOpType op_type) {
    return k * NOpsPerMacronode + static_cast<std::size_t>(op_type);
};

/**
 * @brief Mode index manager for a state object in machinery simulation.
 */
class StateObjModeIdx {
public:
    explicit StateObjModeIdx(const std::size_t n_local_macronodes)
        : n_local_macronodes_(n_local_macronodes),
          cycle_n2_(CreateCycleN2Vector(n_local_macronodes)) {}

    /**
     * @brief Get the mode index of the specified micronode within the State object.
     *
     * @param k Macronode index.
     * @param i The indices (0 through 5) of the six micronodes involved in gate teleportation
     * during operation on macronode k.
     * i=0: micronode b of macronode k.
     * i=1: micronode d of macronode k.
     * i=2: micronode c of macronode k.
     * i=3: micronode a of macronode k.
     * i=4: micronode b of macronode k+1.
     * i=5: micronode d of macronode k+N, where N is the number of macronodes per step.
     * @return std::size_t Index of the specified micronode within the State object.
     * @details In this function, the State object is uniquely determined by the number of
     * macronodes. In machinery simulation, advancing each macronode involves reusing
     * the mode indices in the State object, achieved through repeated index permutation. This
     * permutation can be decomposed into two cycles: Cycle3=(0 4 2) and cycle_n2_.
     */
    std::size_t operator()(const std::size_t k, const std::size_t i) const {
        constexpr auto Cycle3 = std::array<std::size_t, 3>{0, 4, 2};
        switch (i) {
            case 0: return Cycle3[k % 3];
            case 1: return cycle_n2_[k % (n_local_macronodes_ + 2)];
            case 2: return Cycle3[(2 + k) % 3];
            case 3: return cycle_n2_[(k + n_local_macronodes_ + 1) % (n_local_macronodes_ + 2)];
            case 4: return Cycle3[(1 + k) % 3];
            default: return cycle_n2_[(k + i - 4) % (n_local_macronodes_ + 2)];
        }
    }

private:
    /// Number of macronodes per step.
    const std::size_t n_local_macronodes_;

    /// Cycle with N+2 elements: (1 5 6 ... N+3 N+4 3), where N=n_local_macronodes_.
    const std::vector<std::size_t> cycle_n2_;

    static std::vector<std::size_t> CreateCycleN2Vector(const std::size_t n_local_macronodes) {
        auto tmp = std::vector<std::size_t>(n_local_macronodes + 2);
        tmp[0] = 1;
        tmp[n_local_macronodes + 1] = 3;
        for (std::size_t j = 1; j <= n_local_macronodes; ++j) { tmp[j] = 4 + j; }
        return tmp;
    }
};

/**
 * @brief Mode indices of frequently used micronodes in a macronode operation, within the State
 * object.
 */
struct TemporaryStateObjModeIdx {
    std::size_t s0;
    std::size_t s1;
    std::size_t s2;
    std::size_t s3;
    std::size_t s4;
    std::size_t slast;
};

/**
 * @brief Get nonlinear feedforward objects for linear feedforward source parameters.
 *
 * @details For each source parameter index (the four homodyne angles within a macronode) used in
 * linear feedforward, if that parameter is a target of a nonlinear feedforward (NLFF), retrieve the
 * NLFF's FeedForward object. If not, return nullopt.
 *
 * @tparam Float
 * @param macronode_idx Index of the macronode to which the homodyne angles belong.
 * @param circuit Circuit object that contains the nonlinear feedforward objects.
 * @return std::array<const FeedForward<Float>*, 4> The nonlinear feedforward objects.
 */
template <std::floating_point Float>
auto GetNLFFsToHomodyneAngle(const std::size_t macronode_idx, const Circuit<Float>& circuit)
    -> std::array<const FeedForward<Float>*, 4> {
    const auto k = macronode_idx;
    const auto source_param_idxs =
        std::array<ParamIdx, 4>{ParamIdx{MacronodeOpIdx(k, MacronodeOpType::HomodyneA), 0},
                                ParamIdx{MacronodeOpIdx(k, MacronodeOpType::HomodyneB), 0},
                                ParamIdx{MacronodeOpIdx(k, MacronodeOpType::HomodyneC), 0},
                                ParamIdx{MacronodeOpIdx(k, MacronodeOpType::HomodyneD), 0}};
    auto nlffs = std::array<const FeedForward<Float>*, 4>{};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto& ff_target_params = circuit.GetFFTargetParams();
        auto it = ff_target_params.find(source_param_idxs[i]);
        nlffs[i] = it == ff_target_params.end() ? nullptr : it->second;
    }
    return nlffs;
}

/**
 * @brief Calculate FIMac, where the macronode angles are parameterized by nonlinear feedforward.
 *
 * @details Calculate FIMac. The parameter macro_angle (i.e., theta) for FIMac is parameterized by
 * measured values from other macronodes as nonlinear feedforward functions. The specific
 * parameterization is dictated by the input nlff object. How they are parameterized is determined
 * by the input nlff object.
 *
 * @tparam Float
 * @param nlffs Nonlinear feedforward objects for each homodyne angle.
 * @return std::function<std::pair<DisplacementComplex<Float>, DisplacementComplex<Float>>(
 * const std::vector<Float>&)> FIMac whose macronode angles are parameterized by nonlinear
 * feedforward.
 */
template <std::floating_point Float>
auto CreateMParametrizedFIMac(const std::array<const FeedForward<Float>*, 4>& nlffs,
                              const MacronodeAngle<Float>& original_macronode_angles)
    -> std::function<std::pair<DisplacementComplex<Float>, DisplacementComplex<Float>>(
        const std::vector<Float>&)> {
    return [nlffs, original_macronode_angles](
               const auto& m) -> std::pair<DisplacementComplex<Float>, DisplacementComplex<Float>> {
        auto theta_vals = std::array<Float, 4>{};
        for (std::size_t i_theta = 0, i_m = 4; i_theta < 4; ++i_theta) {
            const auto nlff = nlffs[i_theta];
            if (nlff) {
#ifndef NDEBUG
                if (nlff->source_ops_idxs.size() != 4) {
                    throw std::runtime_error("Size of nlff.source_ops_idxs must be 4.");
                }
#endif
                theta_vals[i_theta] = nlff->func({m[i_m], m[i_m + 1], m[i_m + 2], m[i_m + 3]});
                i_m += 4;
            } else {
                theta_vals[i_theta] = original_macronode_angles[i_theta];
            }
        }
        return FIMac(MacronodeAngle{theta_vals[0], theta_vals[1], theta_vals[2], theta_vals[3]},
                     MacronodeMeasuredValue{m[0], m[1], m[2], m[3]});
    };
}

/**
 * @brief Create and return a Circuit object based on the provided machinery representation.
 *
 * @tparam Float
 * @param config Machinery representation.
 * @param squeezing_level Squeezing level (in dB) of the x-squeezed modes in the EPR states utilized
 * for gate teleportation. The squeezing level of the p-squeezed resource modes is the oppsite of
 * this.
 * @return Circuit<Float> Circuit object corresponding to the provided machinery representation.
 */
template <std::floating_point Float>
auto MachineryCircuit(const PhysicalCircuitConfiguration<Float>& config,
                      const Float squeezing_level) -> Circuit<Float> {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::ConvertMachineryToCircuit>{};
#endif
    const auto n_total_macronodes = config.n_local_macronodes * config.n_steps;
    const auto n_local_macronodes = config.n_local_macronodes;
    const auto mode_idx = StateObjModeIdx{n_local_macronodes};

    auto circuit = Circuit<Float>{};

    circuit.GetMutOperations().reserve(NOpsPerMacronode * n_total_macronodes);
    for (std::size_t k = 0; k < n_total_macronodes; ++k) {
        const auto tmp_mode_idx = TemporaryStateObjModeIdx{
            mode_idx(k, 0), mode_idx(k, 1), mode_idx(k, 2),
            mode_idx(k, 3), mode_idx(k, 4), mode_idx(k, n_local_macronodes + 4)};

        // Displacement
        const auto d1 = config.displacements_k_minus_1[k];
        const auto dn = config.displacements_k_minus_n[k];
        circuit.template EmplaceOperation<operation::Displacement<Float>>(tmp_mode_idx.s0, d1.x,
                                                                          d1.p);
        circuit.template EmplaceOperation<operation::Displacement<Float>>(tmp_mode_idx.s1, dn.x,
                                                                          dn.p);

        // Entanglement between (B, k) and (D, k)
        circuit.template EmplaceOperation<operation::util::BeamSplitter50<Float>>(tmp_mode_idx.s0,
                                                                                  tmp_mode_idx.s1);
        // Entanglement between (A, k) and (B, k+1)
        circuit.template EmplaceOperation<operation::util::BeamSplitter50<Float>>(tmp_mode_idx.s3,
                                                                                  tmp_mode_idx.s4);
        // Entanglement between (C, k) and (D, k+N)
        circuit.template EmplaceOperation<operation::util::BeamSplitter50<Float>>(
            tmp_mode_idx.s2, tmp_mode_idx.slast);
        // Entanglement between (B, k) and (A, k)
        circuit.template EmplaceOperation<operation::util::BeamSplitter50<Float>>(tmp_mode_idx.s0,
                                                                                  tmp_mode_idx.s3);
        // Entanglement between (D, k) and (C, k)
        circuit.template EmplaceOperation<operation::util::BeamSplitter50<Float>>(tmp_mode_idx.s1,
                                                                                  tmp_mode_idx.s2);
        // Micronodes measurement
        const auto& macronode_angle = config.homodyne_angles[k];
        circuit.template EmplaceOperation<operation::HomodyneSingleMode<Float>>(
            tmp_mode_idx.s3, macronode_angle.theta_a);
        circuit.template EmplaceOperation<operation::HomodyneSingleMode<Float>>(
            tmp_mode_idx.s0, macronode_angle.theta_b);
        circuit.template EmplaceOperation<operation::HomodyneSingleMode<Float>>(
            tmp_mode_idx.s2, macronode_angle.theta_c);
        circuit.template EmplaceOperation<operation::HomodyneSingleMode<Float>>(
            tmp_mode_idx.s1, macronode_angle.theta_d);
        // Entanglement between (D, k+N) and (B, k+1)
        circuit.template EmplaceOperation<operation::util::BeamSplitter50<Float>>(
            tmp_mode_idx.slast, tmp_mode_idx.s4);

        // Linear feedforward
        circuit.template EmplaceOperation<operation::Displacement<Float>>(tmp_mode_idx.s4, 0, 0);
        circuit.template EmplaceOperation<operation::Displacement<Float>>(tmp_mode_idx.slast, 0, 0);

        // Replace modes
        circuit.template EmplaceOperation<operation::util::ReplaceBySqueezedState<Float>>(
            mode_idx(k + 1, 2), squeezing_level, std::numbers::pi_v<Float> / 2);
        circuit.template EmplaceOperation<operation::util::ReplaceBySqueezedState<Float>>(
            mode_idx(k + 1, 3), squeezing_level, std::numbers::pi_v<Float> / 2);
        circuit.template EmplaceOperation<operation::util::ReplaceBySqueezedState<Float>>(
            mode_idx(k + 1, 4), squeezing_level);
        circuit.template EmplaceOperation<operation::util::ReplaceBySqueezedState<Float>>(
            mode_idx(k + 1, n_local_macronodes + 4), squeezing_level);
    }

    {
        const auto n_ff = n_total_macronodes * 4 + config.nlffs.size();
        circuit.ReserveFFFuncs(n_ff);
        circuit.ReserveFFTargetParams(n_ff);
    }

    // Add non-linear feedforward
    for (const auto& nlff : config.nlffs) {
        const auto k = nlff.from_macronode;
        const auto meas_op_idxs =
            std::vector<std::size_t>{MacronodeOpIdx(k, MacronodeOpType::HomodyneA),
                                     MacronodeOpIdx(k, MacronodeOpType::HomodyneB),
                                     MacronodeOpIdx(k, MacronodeOpType::HomodyneC),
                                     MacronodeOpIdx(k, MacronodeOpType::HomodyneD)};
        const auto& tmp_from_abcd = nlff.from_abcd;
        const auto& tmp_function = config.functions[nlff.function];
        auto ff_func = FFFunc<Float>{[tmp_from_abcd, tmp_function](const auto& m) -> Float {
#ifndef NDEBUG
            if (m.size() != 4) { throw std::runtime_error("Invalid size of m"); }
#endif
            Float m_readout = 0.0;
            switch (tmp_from_abcd) {  // m -> M
                case 0: m_readout = (m[0] - m[1] + m[2] - m[3]) / 2; break;
                case 1: m_readout = (m[0] + m[1] + m[2] + m[3]) / 2; break;
                case 2: m_readout = (-m[0] + m[1] + m[2] - m[3]) / 2; break;
                case 3: m_readout = (-m[0] - m[1] + m[2] + m[3]) / 2; break;
                default: throw std::runtime_error("Invalid from_abcd");
            }
            return tmp_function({m_readout});
        }};
        ParamIdx target_param;
        switch (nlff.to_parameter) {
            case 0:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::HomodyneA), 0};
                break;
            case 1:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::HomodyneB), 0};
                break;
            case 2:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::HomodyneC), 0};
                break;
            case 3:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::HomodyneD), 0};
                break;
            case 4:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::DisplacementB),
                                0};
                break;
            case 5:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::DisplacementB),
                                1};
                break;
            case 6:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::DisplacementD),
                                0};
                break;
            case 7:
                target_param = {MacronodeOpIdx(nlff.to_macronode, MacronodeOpType::DisplacementD),
                                1};
                break;
            default: throw std::runtime_error("Invalid to_parameter");
        }
        auto ff_obj = FeedForward<Float>{meas_op_idxs, target_param, ff_func};
        circuit.AddFF(std::move(ff_obj));
    }

    // Add linear feedforward for gate teleportation
    for (std::size_t k = 0; k < n_total_macronodes; ++k) {
        const auto nlffs = GetNLFFsToHomodyneAngle(k, circuit);

        auto source_op_idxs =
            std::vector<std::size_t>{MacronodeOpIdx(k, MacronodeOpType::HomodyneA),
                                     MacronodeOpIdx(k, MacronodeOpType::HomodyneB),
                                     MacronodeOpIdx(k, MacronodeOpType::HomodyneC),
                                     MacronodeOpIdx(k, MacronodeOpType::HomodyneD)};
        for (const auto nlff : nlffs) {  // insert source operation indices of NLFF
            if (nlff) {
                std::ranges::copy(nlff->source_ops_idxs, std::back_inserter(source_op_idxs));
            }
        }

        const auto ff_disp_op_idx_b = MacronodeOpIdx(k, MacronodeOpType::LinearFFB);
        const auto ff_disp_op_idx_d = MacronodeOpIdx(k, MacronodeOpType::LinearFFD);
        auto fi_mac = CreateMParametrizedFIMac(nlffs, config.homodyne_angles[k]);
        switch (config.generating_method_for_ff_coeff_k_plus_1[k]) {
            case PbFeedForwardCoefficientGenerationMethod::
                FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_FROM_HOMODYNE_ANGLES: {
                auto fi_mac_1x =
                    FFFunc<Float>{[fi_mac](const auto& m) -> Float { return fi_mac(m).first.x; }};
                auto fi_mac_1p =
                    FFFunc<Float>{[fi_mac](const auto& m) -> Float { return fi_mac(m).first.p; }};
                auto ff_1x =
                    FeedForward<Float>{source_op_idxs, ParamIdx{ff_disp_op_idx_b, 0}, fi_mac_1x};
                circuit.AddFF(std::move(ff_1x));
                auto ff_1p =
                    FeedForward<Float>{source_op_idxs, ParamIdx{ff_disp_op_idx_b, 1}, fi_mac_1p};
                circuit.AddFF(std::move(ff_1p));
            } break;
            case PbFeedForwardCoefficientGenerationMethod::
                FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED:
                break;
            default:
                throw std::runtime_error(
                    "Generating method for feedforward to k+1 is not specified correctly.");
        }
        switch (config.generating_method_for_ff_coeff_k_plus_n[k]) {
            case PbFeedForwardCoefficientGenerationMethod::
                FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_FROM_HOMODYNE_ANGLES: {
                auto fi_mac_nx =
                    FFFunc<Float>{[fi_mac](const auto& m) -> Float { return fi_mac(m).second.x; }};
                auto fi_mac_np =
                    FFFunc<Float>{[fi_mac](const auto& m) -> Float { return fi_mac(m).second.p; }};
                auto ff_nx = FeedForward<Float>{source_op_idxs, {ff_disp_op_idx_d, 0}, fi_mac_nx};
                circuit.AddFF(std::move(ff_nx));
                auto ff_np = FeedForward<Float>{source_op_idxs, {ff_disp_op_idx_d, 1}, fi_mac_np};
                circuit.AddFF(std::move(ff_np));
            } break;
            case PbFeedForwardCoefficientGenerationMethod::
                FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED:
                break;
            default:
                throw std::runtime_error(
                    "Generating method for feedforward to k+N is not specified correctly.");
        }
    }

    return circuit;
}
}  // namespace bosim::machinery
