// SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

/*
Newton Raphson Power Flow

****** Voltage
U_i = V_i * exp(1j * theta_i) = U_i_r + 1j * U_i_i
U_i_r = V_i * cos(theta_i)
U_i_i = V_i * sin(theta_i)

****** Admittance matrix
Yij = Gij + 1j * Bij

****** Object function:

f(theta, V) = PQ_sp - PQ_cal = del_pq = 0
sp = specified
cal = calculated

PQ_sp = [P_sp_0, Q_sp_0, P_sp_1, Q_sp_1, ...]^T
PQ_cal = [P_cal_0, Q_cal_0, P_cal_1, Q_cal_1, ...]^T


****** Solution: use Newton-Raphson iteration
The modified Jacobian derivative
Jf = [
     [Jf00, Jf01, Jf02, ..., ]
     [Jf10, Jf11, Jf12, ..., ],
     ...
    ]

J = -Jf
  J_ij =
     [[dP_cal_i/dtheta_j, dP_cal_i/dV_j * V_j],
      [dQ_cal_i/dtheta_j, dQ_cal_i/dV_j * V_j]]
    -
     [[dP_sp_i/dtheta_j, dP_sp_i/dV_j * V_j],
      [dQ_sp_i/dtheta_j, dQ_sp_i/dV_j * V_j]]

Iteration increment
del_x = [del_theta_0, del_V_0/V_0, del_theta_1, del_V_1/V_1, ...]^T = - (Jf)^-1 * del_pq = J^-1 * del_pq

theta_i_(k+1) = theta_i_(k) + del_theta_i
V_i_(k+1) = V_i_k + (del_V_i/V_i) * V_i


****** Calculation process
set J[...] = 0
set del_pq[...] = 0


*** intermediate variable H, N, M, L into J, as incomplete J

symbol @* : outer product of two vectors https://en.wikipedia.org/wiki/Outer_product
     x1 = [a, b]^T, x2 = [c, d]^T, x1 @* x2 = [[ac, ad], [bc, bd]]
symbol .* : elementwise multiply of two matrices/tensors or vector

theta_ij =
    for symmetric: theta_i - theta_j
    for asymmetric: [
                     [theta_i_a - theta_j_a, theta_i_a - theta_j_b, theta_i_a - theta_j_c],
                     [theta_i_b - theta_j_a, theta_i_b - theta_j_b, theta_i_b - theta_j_c],
                     [theta_i_c - theta_j_a, theta_i_c - theta_j_b, theta_i_c - theta_j_c]
                    ]
diag(Vi) * cos(theta_ij) * diag(Vj) = Ui_r @* Uj_r + Ui_i @* Uj_i = cij
diag(Vi) * sin(theta_ij) * diag(Vj) = Ui_i @* Uj_r - Ui_r @* Uj_i = sij

Hij = diag(Vi) * ( Gij .* sin(theta_ij) - Bij .* cos(theta_ij) ) * diag(Vj)
    = Gij .* sij - Bij .* cij
Nij = diag(Vi) * ( Gij .* cos(Theta_ij) + Bij .* sin(Theta_ij) ) * diag(Vj)
    = Gij .* cij + Bij .* sij
Mij = - Nij
Lij = Hij

save to J_ij = [
                   [Hij, Nij],
                   [Mij, Lij]
               ]

*** PQ_cal
P_cal_i = sum{j} (Nij * I)
Q_cal_i = sum{j} (Hij * I)
I =
    symmetric: 1
    asymmetric: [1, 1, 1]^T
set
del_pq_i = -[P_cal_i, Q_cal_i]

*** modify J into complete Jacobian for PQ_cal
correction for diagonal
Jii.H += diag(-Q_cal_i)
Jii.N -= diag(-P_cal_i)
Jii.M -= diag(-P_i)
Jii.L -= diag(-Q_cal_i)

*** calculate PQ_sp, and dPQ_sp/(dtheta, dV)

** for load/generation
PQ_sp =
    PQ_base for constant pq
    PQ_base * V for constant i
    PQ_base * V^2 for constant z
del_pq += PQ_sp

dPQ_sp/dtheta = 0
dPQ_sp/dV_i * V =
    0 for constant pq
    PQ_base * V for constant i (dPQ_sp/dV = PQ_base)
    PQ_base * 2 * V^2 for constant z (dPQ_sp/dV = PQ_base * 2 * V)
J.N -= diag(dP_sp/dV .* V)
J.L -= diag(dQ_sp/dV .* V)

** for source
A mini two bus equivalent system is built to solve the problem

bus_m (network) ---Y--- bus_s (voltage_source)
element_admittance matrix [[Y -Y], [-Y, Y]]
voltage at bus_s is known as U_ref
voltage at bus_m is U_m

The PQ_sp for the bus_m in the network, is in this case the negative of the power injection for this fictional 2-bus
network

* Calculate HNML for mm, ms, using the same formula, then calculate the other quantities
P_cal_m = (Nmm + Nms) * I
Q_cal_m = (Hmm + Hms) * I
dP_cal_m/dtheta = Hmm - diag(Q_cal_m)
dP_cal_m/dV = Nmm + diag(P_cal_m)
dQ_cal_m/dtheta = Mmm + diag(P_cal_m)
dQ_cal_m/dV = Lmm + diag(Q_cal_m)

* negate the value and add into the main matrices
PQ_sp = -PQ_cal_m
del_pq -= PQ_cal_m

J.H -= -dP_cal_m/dtheta
J.N -= -dP_cal_m/dV
J.M -= -dQ_cal_m/dtheta
J.L -= -dQ_cal_m/dV

*/

#include "block_matrix.hpp"
#include "iterative_pf_solver.hpp"
#include "sparse_lu_solver.hpp"
#include "y_bus.hpp"

#include "../calculation_parameters.hpp"
#include "../common/common.hpp"
#include "../common/exception.hpp"
#include "../common/three_phase_tensor.hpp"
#include "../common/timer.hpp"

namespace power_grid_model::math_solver {

// hide implementation in inside namespace
namespace newton_raphson_pf {

// class for phasor in polar coordinate and/or complex power
template <symmetry_tag sym> struct PolarPhasor : public Block<double, sym, false, 2> {
    template <int r, int c> using GetterType = typename Block<double, sym, false, 2>::template GetterType<r, c>;

    // eigen expression
    using Block<double, sym, false, 2>::Block;
    using Block<double, sym, false, 2>::operator=;

    GetterType<0, 0> theta() { return this->template get_val<0, 0>(); }
    GetterType<1, 0> v() { return this->template get_val<1, 0>(); }

    GetterType<0, 0> p() { return this->template get_val<0, 0>(); }
    GetterType<1, 0> q() { return this->template get_val<1, 0>(); }
};

// class for complex power
template <symmetry_tag sym> using ComplexPower = PolarPhasor<sym>;

// class of pf block
// block of incomplete power flow jacobian
// non-diagonal H, N, M, L
// [ [H = dP/dTheta, N = V * dP/dV],
// [M = dQ/dTheta = -N, L = V * dQ/dV = H] ]
// Hij = Gij .* sij - Bij .* cij = L
// Nij = Gij .* cij + Bij .* sij = -M
template <symmetry_tag sym> class PFJacBlock : public Block<double, sym, true, 2> {
  public:
    template <int r, int c> using GetterType = typename Block<double, sym, true, 2>::template GetterType<r, c>;

    // eigen expression
    using Block<double, sym, true, 2>::Block;
    using Block<double, sym, true, 2>::operator=;

    GetterType<0, 0> h() { return this->template get_val<0, 0>(); }
    GetterType<0, 1> n() { return this->template get_val<0, 1>(); }
    GetterType<1, 0> m() { return this->template get_val<1, 0>(); }
    GetterType<1, 1> l() { return this->template get_val<1, 1>(); }
};

// solver
template <symmetry_tag sym_type>
class NewtonRaphsonPFSolver : public IterativePFSolver<sym_type, NewtonRaphsonPFSolver<sym_type>> {
  public:
    using sym = sym_type;

    using SparseSolverType = SparseLUSolver<PFJacBlock<sym>, ComplexPower<sym>, PolarPhasor<sym>>;
    using BlockPermArray =
        typename SparseLUSolver<PFJacBlock<sym>, ComplexPower<sym>, PolarPhasor<sym>>::BlockPermArray;

    static constexpr auto is_iterative = true;

    NewtonRaphsonPFSolver(YBus<sym> const& y_bus, std::shared_ptr<MathModelTopology const> const& topo_ptr)
        : IterativePFSolver<sym, NewtonRaphsonPFSolver>{y_bus, topo_ptr},
          data_jac_(y_bus.nnz_lu()),
          x_(y_bus.size()),
          del_x_pq_(y_bus.size()),
          sparse_solver_{y_bus.shared_indptr_lu(), y_bus.shared_indices_lu(), y_bus.shared_diag_lu()},
          perm_(y_bus.size()) {}

    // Initilize the unknown variable in polar form
    void initialize_derived_solver(YBus<sym> const& y_bus, PowerFlowInput<sym> const& input,
                                   SolverOutput<sym>& output) {
        using LinearSparseSolverType = SparseLUSolver<ComplexTensor<sym>, ComplexValue<sym>, ComplexValue<sym>>;

        ComplexTensorVector<sym> linear_mat_data(y_bus.nnz_lu());
        LinearSparseSolverType linear_sparse_solver{y_bus.shared_indptr_lu(), y_bus.shared_indices_lu(),
                                                    y_bus.shared_diag_lu()};
        typename LinearSparseSolverType::BlockPermArray linear_perm(y_bus.size());

        detail::copy_y_bus<sym>(y_bus, linear_mat_data);
        detail::prepare_linear_matrix_and_rhs(y_bus, input, *this->load_gens_per_bus_, *this->sources_per_bus_, output,
                                              linear_mat_data);
        linear_sparse_solver.prefactorize_and_solve(linear_mat_data, linear_perm, output.u, output.u);

        // get magnitude and angle of start voltage
        for (Idx i = 0; i != this->n_bus_; ++i) {
            x_[i].v() = cabs(output.u[i]);
            x_[i].theta() = arg(output.u[i]);
        }
    }

    // Calculate the Jacobian and deviation
    void prepare_matrix_and_rhs(YBus<sym> const& y_bus, PowerFlowInput<sym> const& input,
                                ComplexValueVector<sym> const& u) {
        std::vector<LoadGenType> const& load_gen_type = *this->load_gen_type_;
        IdxVector const& bus_entry = y_bus.lu_diag();

        prepare_matrix_and_rhs_from_network_perspective(y_bus, u, bus_entry);

        for (auto const& [bus_number, load_gens, sources] :
             enumerated_zip_sequence(*this->load_gens_per_bus_, *this->sources_per_bus_)) {
            Idx const diagonal_position = bus_entry[bus_number];
            add_loads(load_gens, bus_number, diagonal_position, input, load_gen_type);
            add_sources(sources, bus_number, diagonal_position, y_bus, input, u);
        }
    }

    // Solve the linear Equations
    void solve_matrix() { sparse_solver_.prefactorize_and_solve(data_jac_, perm_, del_x_pq_, del_x_pq_); }

    // Get maximum deviation among all bus voltages
    double iterate_unknown(ComplexValueVector<sym>& u) {
        double max_dev = 0.0;
        // loop each bus as i
        for (Idx i = 0; i != this->n_bus_; ++i) {
            // angle
            x_[i].theta() += del_x_pq_[i].theta();
            // magnitude
            x_[i].v() += x_[i].v() * del_x_pq_[i].v();
            // temporary complex phasor
            // U = V * exp(1i*theta)
            ComplexValue<sym> const& u_tmp = x_[i].v() * exp(1.0i * x_[i].theta());
            // get dev of last iteration, get max
            double const dev = max_val(cabs(u_tmp - u[i]));
            max_dev = std::max(dev, max_dev);
            // assign
            u[i] = u_tmp;
        }
        return max_dev;
    }

  private:
    // data for jacobian
    std::vector<PFJacBlock<sym>> data_jac_;
    // calculation data
    std::vector<PolarPhasor<sym>> x_; // unknown
    // this stores in different steps
    // 1. negative power injection: - p/q_calculated
    // 2. power unbalance: p/q_specified - p/q_calculated
    // 3. unknown iterative
    std::vector<ComplexPower<sym>> del_x_pq_;

    SparseSolverType sparse_solver_;
    // permutation array
    BlockPermArray perm_;

    /// @brief power_flow_ij = (ui @* conj(uj))  .* conj(yij)
    /// Hij = diag(Vi) * ( Gij .* sin(theta_ij) - Bij .* cos(theta_ij) ) * diag(Vj)
    /// = imaginary(power_flow_ij)
    /// Nij = diag(Vi) * ( Gij .* cos(theta_ij) + Bij .* sin(theta_ij) ) * diag(Vj)
    /// = real(power_flow_ij)
    /// Mij = -Nij
    /// Lij = Hij
    static PFJacBlock<sym> calculate_hnml(ComplexTensor<sym> const& yij, ComplexValue<sym> const& ui,
                                          ComplexValue<sym> const& uj) {
        PFJacBlock<sym> block{};
        ComplexTensor<sym> const power_flow_ij = vector_outer_product(ui, conj(uj)) * conj(yij);
        block.h() = imag(power_flow_ij);
        block.n() = real(power_flow_ij);
        block.m() = -block.n();
        block.l() = block.h();
        return block;
    }

    void prepare_matrix_and_rhs_from_network_perspective(YBus<sym> const& y_bus, ComplexValueVector<sym> const& u,
                                                         IdxVector const& bus_entry) {
        IdxVector const& indptr = y_bus.row_indptr_lu();
        IdxVector const& indices = y_bus.col_indices_lu();
        IdxVector const& map_lu_y_bus = y_bus.map_lu_y_bus();
        ComplexTensorVector<sym> const& ydata = y_bus.admittance();

        for (Idx row = 0; row != this->n_bus_; ++row) {
            // reset power injection
            del_x_pq_[row].p() = RealValue<sym>{0.0};
            del_x_pq_[row].q() = RealValue<sym>{0.0};
            // loop for column for incomplete jacobian and injection
            // k as data indices
            // j as column indices
            for (Idx k = indptr[row]; k != indptr[row + 1]; ++k) {
                // set to zero and skip if it is a fill-in
                Idx const k_y_bus = map_lu_y_bus[k];
                if (k_y_bus == -1) {
                    data_jac_[k] = PFJacBlock<sym>{};
                    continue;
                }
                Idx const j = indices[k];
                // incomplete jacobian
                data_jac_[k] = calculate_hnml(ydata[k_y_bus], u[row], u[j]);
                // accumulate negative power injection
                // -P = sum(-N)
                del_x_pq_[row].p() -= sum_row(data_jac_[k].n());
                // -Q = sum (-H)
                del_x_pq_[row].q() -= sum_row(data_jac_[k].h());
            }
            // correct diagonal part of jacobian
            Idx const k = bus_entry[row];
            // diagonal correction
            // del_pq has negative injection
            // H += (-Q)
            add_diag(data_jac_[k].h(), del_x_pq_[row].q());
            // N -= (-P)
            add_diag(data_jac_[k].n(), -del_x_pq_[row].p());
            // M -= (-P)
            add_diag(data_jac_[k].m(), -del_x_pq_[row].p());
            // L -= (-Q)
            add_diag(data_jac_[k].l(), -del_x_pq_[row].q());
        }
    }

    void add_loads(IdxRange const& load_gens, Idx bus_number, Idx diagonal_position, PowerFlowInput<sym> const& input,
                   std::vector<LoadGenType> const& load_gen_type) {
        using enum LoadGenType;
        for (Idx const load_number : load_gens) {
            LoadGenType const type = load_gen_type[load_number];
            // modify jacobian and del_pq based on type
            switch (type) {
            case const_pq:
                add_const_power_load(bus_number, load_number, input);
                break;
            case const_y:
                add_const_impedance_load(bus_number, load_number, diagonal_position, input);
                break;
            case const_i:
                add_const_current_load(bus_number, load_number, diagonal_position, input);
                break;
            default:
                throw MissingCaseForEnumError("Jacobian and deviation calculation", type);
            }
        }
    }

    void add_const_power_load(Idx bus_number, Idx load_number, PowerFlowInput<sym> const& input) {
        // PQ_sp = PQ_base
        del_x_pq_[bus_number].p() += real(input.s_injection[load_number]);
        del_x_pq_[bus_number].q() += imag(input.s_injection[load_number]);
        // -dPQ_sp/dV * V = 0
    }

    void add_const_impedance_load(Idx bus_number, Idx load_number, Idx diagonal_position,
                                  PowerFlowInput<sym> const& input) {
        // PQ_sp = PQ_base * V^2
        del_x_pq_[bus_number].p() += real(input.s_injection[load_number]) * x_[bus_number].v() * x_[bus_number].v();
        del_x_pq_[bus_number].q() += imag(input.s_injection[load_number]) * x_[bus_number].v() * x_[bus_number].v();
        // -dPQ_sp/dV * V = -PQ_base * 2 * V^2
        add_diag(data_jac_[diagonal_position].n(),
                 -real(input.s_injection[load_number]) * 2.0 * x_[bus_number].v() * x_[bus_number].v());
        add_diag(data_jac_[diagonal_position].l(),
                 -imag(input.s_injection[load_number]) * 2.0 * x_[bus_number].v() * x_[bus_number].v());
    }

    void add_const_current_load(Idx bus_number, Idx load_number, Idx diagonal_position,
                                PowerFlowInput<sym> const& input) {
        // PQ_sp = PQ_base * V
        del_x_pq_[bus_number].p() += real(input.s_injection[load_number]) * x_[bus_number].v();
        del_x_pq_[bus_number].q() += imag(input.s_injection[load_number]) * x_[bus_number].v();
        // -dPQ_sp/dV * V = -PQ_base * V
        add_diag(data_jac_[diagonal_position].n(), -real(input.s_injection[load_number]) * x_[bus_number].v());
        add_diag(data_jac_[diagonal_position].l(), -imag(input.s_injection[load_number]) * x_[bus_number].v());
    }

    void add_sources(IdxRange const& sources, Idx bus_number, Idx diagonal_position, YBus<sym> const& y_bus,
                     PowerFlowInput<sym> const& input, ComplexValueVector<sym> const& u) {
        for (Idx const source_number : sources) {
            ComplexTensor<sym> const y_ref = y_bus.math_model_param().source_param[source_number].template y_ref<sym>();
            ComplexValue<sym> const u_ref{input.source[source_number]};
            // calculate block, um = ui, us = uref
            PFJacBlock<sym> block_mm = calculate_hnml(y_ref, u[bus_number], u[bus_number]);
            PFJacBlock<sym> block_ms = calculate_hnml(-y_ref, u[bus_number], u_ref);
            // P_cal_m = (Nmm + Nms) * I
            RealValue<sym> const p_cal = sum_row(block_mm.n() + block_ms.n());
            // Q_cal_m = (Hmm + Hms) * I
            RealValue<sym> const q_cal = sum_row(block_mm.h() + block_ms.h());
            // correct hnml for mm
            add_diag(block_mm.h(), -q_cal);
            add_diag(block_mm.n(), p_cal);
            add_diag(block_mm.m(), p_cal);
            add_diag(block_mm.l(), q_cal);
            // append to del_pq
            del_x_pq_[bus_number].p() -= p_cal;
            del_x_pq_[bus_number].q() -= q_cal;
            // append to jacobian block
            // hnml -= -dPQ_cal/(dtheta,dV)
            // hnml += dPQ_cal/(dtheta,dV)
            data_jac_[diagonal_position].h() += block_mm.h();
            data_jac_[diagonal_position].n() += block_mm.n();
            data_jac_[diagonal_position].m() += block_mm.m();
            data_jac_[diagonal_position].l() += block_mm.l();
        }
    }
};

} // namespace newton_raphson_pf

using newton_raphson_pf::NewtonRaphsonPFSolver;

} // namespace power_grid_model::math_solver
