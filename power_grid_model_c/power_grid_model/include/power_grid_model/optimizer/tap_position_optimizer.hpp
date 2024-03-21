// SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "base_optimizer.hpp"

#include "../auxiliary/dataset.hpp"
#include "../common/enum.hpp"

#include <boost/graph/compressed_sparse_row_graph.hpp>

namespace power_grid_model::optimizer {
namespace tap_position_optimizer {

namespace detail = power_grid_model::optimizer::detail;

using GraphIdx = size_t;
struct Edge {
    Idx2D pos{};
    Idx weight{};
};

// TODO(mgovers): investigate whether this really is the correct graph structure
using TransformerGraph = boost::compressed_sparse_row_graph<boost::directedS, boost::no_property, Edge,
                                                            boost::no_property, GraphIdx, GraphIdx>;

template <main_core::main_model_state_c State>
inline auto build_transformer_graph(State const& /*state*/) -> TransformerGraph {
    // TODO(mgovers): implement
    return {};
}

inline auto determine_distance_from_source(TransformerGraph const& /*graph*/) -> std::vector<std::pair<Idx2D, Idx>> {
    // TODO(mgovers): implement
    return {};
}

inline auto rank_transformers(std::vector<std::pair<Idx2D, Idx>> const& /*distances*/) -> std::vector<Idx2D> {
    // TODO(mgovers): rank Idx2D of transformers as listed in the container
    return {};
}

template <main_core::main_model_state_c State> inline auto rank_transformers(State const& state) -> std::vector<Idx2D> {
    return rank_transformers(determine_distance_from_source(build_transformer_graph(state)));
}

template <typename StateCalculator, typename StateUpdater_, typename State_>
    requires detail::steady_state_calculator_c<StateCalculator, State_> &&
             std::invocable<std::remove_cvref_t<StateUpdater_>, ConstDataset const&>
class TapPositionOptimizer : public detail::BaseOptimizer<StateCalculator, State_> {
  public:
    using Base = detail::BaseOptimizer<StateCalculator, State_>;
    using typename Base::Calculator;
    using typename Base::ResultType;
    using typename Base::State;
    using StateUpdater = StateUpdater_;

    TapPositionOptimizer(Calculator calculate, StateUpdater update, OptimizerStrategy strategy)
        : calculate_{std::move(calculate)}, update_{std::move(update)}, strategy_{strategy} {}

    auto optimize(State const& state) -> ResultType final {
        auto const order = rank_transformers(state);
        return optimize(state, order);
    }

    constexpr auto strategy() { return strategy_; }

  private:
    auto optimize(State const& /*state*/, std::vector<Idx2D> const& /*order*/) -> ResultType {
        // TODO(mgovers): rank Idx2D of transformers as listed in the container
        throw PowerGridError{};
    }

    Calculator calculate_;
    StateUpdater update_;
    OptimizerStrategy strategy_;
};

} // namespace tap_position_optimizer

template <typename StateCalculator, typename StateUpdater, typename State>
using TapPositionOptimizer = tap_position_optimizer::TapPositionOptimizer<StateCalculator, StateUpdater, State>;

} // namespace power_grid_model::optimizer
