// SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "base_optimizer.hpp"

#include "../all_components.hpp"
#include "../auxiliary/dataset.hpp"
#include "../common/enum.hpp"
#include "../common/exception.hpp"
#include "../main_core/state_queries.hpp"

#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <functional>
#include <queue>
#include <vector>

namespace power_grid_model::optimizer {
namespace tap_position_optimizer {

namespace detail = power_grid_model::optimizer::detail;

using TrafoGraphIdx = Idx;
using EdgeWeight = int64_t;
constexpr auto infty = std::numeric_limits<Idx>::max();

struct TrafoGraphVertex {
    bool is_source{}; // is_source = true if the vertex is a source
};

struct TrafoGraphEdge {
    bool is_trafo{false};
    Idx2D from_to{};
    EdgeWeight weight{};

    bool operator==(const TrafoGraphEdge& other) const {
        return is_trafo == other.is_trafo && from_to == other.from_to && weight == other.weight;
    }

    bool operator<(const TrafoGraphEdge& other) const {
        if (weight != other.weight) {
            return weight < other.weight;
        }
        if (from_to.group != other.from_to.group) {
            return from_to.group < other.from_to.group;
        }
        return from_to.pos < other.from_to.pos;
    }
};

using TrafoGraphEdges = std::vector<std::pair<TrafoGraphIdx, TrafoGraphIdx>>;
using TrafoGraphEdgeProperties = std::vector<TrafoGraphEdge>;
using WeightedTrafoList = std::vector<TrafoGraphEdge>;
using RankedTransformerGroups = std::vector<std::vector<Idx2D>>;

struct RegulatedObjects {
    std::set<Idx> transformers{};
    std::set<Idx> transformers3w{};
};

using TransformerGraph = boost::compressed_sparse_row_graph<boost::directedS, TrafoGraphVertex, TrafoGraphEdge,
                                                            boost::no_property, TrafoGraphIdx, TrafoGraphIdx>;

template <std::same_as<ThreeWindingTransformer> Component, class ComponentContainer>
    requires main_core::model_component_state_c<main_core::MainModelState, ComponentContainer, Component>
constexpr void add_edges(main_core::MainModelState<ComponentContainer> const& state,
                         RegulatedObjects const& regulated_objects, TrafoGraphEdges& edges,
                         TrafoGraphEdgeProperties& edge_props) {
    std::array<std::tuple<Branch3Side, Branch3Side>, 3> const branch3_combinations{
        {{Branch3Side::side_1, Branch3Side::side_2},
         {Branch3Side::side_2, Branch3Side::side_3},
         {Branch3Side::side_1, Branch3Side::side_3}}};
    for (auto const& transformer3w : state.components.template citer<ThreeWindingTransformer>()) {
        for (auto const& [from_side, to_side] : branch3_combinations) {
            if (!transformer3w.status(from_side) || !transformer3w.status(to_side)) {
                continue;
            }
            auto const& from_node = transformer3w.node(from_side);
            auto const& to_node = transformer3w.node(to_side);

            auto const tap_at_from_side = transformer3w.tap_side() == from_side;
            auto const single_direction_condition = regulated_objects.transformers3w.contains(transformer3w.id()) &&
                                                    (tap_at_from_side || transformer3w.tap_side() == to_side);
            if (single_direction_condition) {
                auto const& tap_from = tap_at_from_side ? from_node : to_node;
                auto const& tap_to = tap_at_from_side ? to_node : from_node;
                create_edge(edges, edge_props, tap_from, tap_to, {true, {tap_from, tap_to}, 1});
            } else {
                create_edge(edges, edge_props, from_node, to_node, {true, {from_node, to_node}, 1});
                create_edge(edges, edge_props, to_node, from_node, {true, {to_node, from_node}, 1});
            }
        }
    }
}

template <std::same_as<Transformer> Component, class ComponentContainer>
    requires main_core::model_component_state_c<main_core::MainModelState, ComponentContainer, Component>
constexpr void add_edges(main_core::MainModelState<ComponentContainer> const& state,
                         RegulatedObjects const& regulated_objects, TrafoGraphEdges& edges,
                         TrafoGraphEdgeProperties& edge_props) {
    for (auto const& transformer : state.components.template citer<Transformer>()) {
        if (!transformer.from_status() || !transformer.to_status()) {
            continue;
        }
        auto const& from_node = transformer.from_node();
        auto const& to_node = transformer.to_node();

        if (regulated_objects.transformers.contains(transformer.id())) {
            auto const tap_at_from_side = transformer.tap_side() == BranchSide::from;
            auto const& from_pos = tap_at_from_side ? from_node : to_node;
            auto const& to_pos = tap_at_from_side ? to_node : from_node;
            if (get_component<Node>(state, from_pos).u_rated() < get_component<Node>(state, to_pos).u_rated()) {
                throw AutomaticTapCalculationError(transformer.id());
            }
            create_edge(edges, edge_props, from_pos, to_pos, {true, {from_pos, to_pos}, 1});
        } else {
            create_edge(edges, edge_props, from_node, to_node, {true, {from_node, to_node}, 1});
            create_edge(edges, edge_props, to_node, from_node, {true, {to_node, from_node}, 1});
        }
    }
}

template <typename Component>
concept non_regulating_branch_c = std::same_as<Link, Component> || std::same_as<Line, Component>;

template <non_regulating_branch_c Component, class ComponentContainer>
    requires main_core::model_component_state_c<main_core::MainModelState, ComponentContainer, Component>
constexpr void add_edges(main_core::MainModelState<ComponentContainer> const& state,
                         RegulatedObjects const& /* regulated_objects */, TrafoGraphEdges& edges,
                         TrafoGraphEdgeProperties& edge_props) {
    auto const& iter = state.components.template citer<Component>();
    edges.reserve(std::distance(iter.begin(), iter.end()) * 2);
    edge_props.reserve(std::distance(iter.begin(), iter.end()) * 2);
    for (auto const& branch : iter) {
        if (!branch.from_status() || !branch.to_status()) {
            continue;
        }
        create_edge(edges, edge_props, branch.from_node(), branch.to_node(),
                    {false, {branch.from_node(), branch.to_node()}, 0});
        create_edge(edges, edge_props, branch.to_node(), branch.from_node(),
                    {false, {branch.to_node(), branch.from_node()}, 0});
    }
}

inline void create_edge(TrafoGraphEdges& edges, TrafoGraphEdgeProperties& edge_props, Idx const& start, Idx const& end,
                        TrafoGraphEdge const& edge_prop) {
    edges.emplace_back(static_cast<TrafoGraphIdx>(start), static_cast<TrafoGraphIdx>(end));
    edge_props.emplace_back(edge_prop);
}

template <main_core::main_model_state_c State>
inline void retrieve_regulator_info(State const& state, RegulatedObjects& regulated_objects) {
    for (auto const& regulator : state.components.template citer<TransformerTapRegulator>()) {
        if (!regulator.status()) {
            continue;
        }
        if (regulator.regulated_object_type() == ComponentType::branch) {
            regulated_objects.transformers.emplace(regulator.regulated_object());
        } else {
            regulated_objects.transformers3w.emplace(regulator.regulated_object());
        }
    }
}

template <main_core::main_model_state_c State>
inline auto build_transformer_graph(State const& state) -> TransformerGraph {
    TrafoGraphEdges edges;
    TrafoGraphEdgeProperties edge_props;
    RegulatedObjects regulated_objects;

    if (state.components.template size<TransformerTapRegulator>() > 0) {
        retrieve_regulator_info(state, regulated_objects);
    }
    if (state.components.template size<Transformer>() > 0) {
        add_edges<Transformer>(state, regulated_objects, edges, edge_props);
    }
    if (state.components.template size<ThreeWindingTransformer>() > 0) {
        add_edges<ThreeWindingTransformer>(state, regulated_objects, edges, edge_props);
    }
    if (state.components.template size<Line>() > 0) {
        add_edges<Line>(state, regulated_objects, edges, edge_props);
    }
    if (state.components.template size<Link>() > 0) {
        add_edges<Link>(state, regulated_objects, edges, edge_props);
    }

    // build graph
    TransformerGraph trafo_graph{boost::edges_are_unsorted_multi_pass, edges.cbegin(), edges.cend(),
                                 edge_props.cbegin(),
                                 static_cast<TrafoGraphIdx>(state.components.template size<Node>())};

    BGL_FORALL_VERTICES(v, trafo_graph, TransformerGraph) {
        trafo_graph[v].is_source = false;
    }

    if (state.components.template size<Source>() == 0) {
        return trafo_graph;
    }

    // Mark sources
    for (auto const& source : state.components.template citer<Source>()) {
        // ignore disabled sources
        trafo_graph[source.node()].is_source = source.status();
    }

    return trafo_graph;
}

inline auto process_edges_dijkstra(Idx v, std::vector<EdgeWeight>& edge_weight, std::vector<Idx2D>& edge_from_to,
                                   std::vector<bool>& edge_is_trafo, TransformerGraph const& graph) -> void {
    using TrafoGraphElement = std::pair<EdgeWeight, TrafoGraphIdx>;
    std::priority_queue<TrafoGraphElement, std::vector<TrafoGraphElement>, std::greater<>> pq;
    edge_weight[v] = 0;
    edge_from_to[v] = {v, v};
    pq.push({0, v});

    while (!pq.empty()) {
        auto [dist, u] = pq.top();
        pq.pop();

        if (dist != edge_weight[u]) {
            continue;
        }

        BGL_FORALL_OUTEDGES(u, e, graph, TransformerGraph) {
            auto v = boost::target(e, graph);
            const EdgeWeight weight = graph[e].weight;

            if (edge_weight[u] + weight < edge_weight[v]) {
                edge_weight[v] = edge_weight[u] + weight;
                edge_from_to[v] = graph[e].from_to;
                edge_is_trafo[v] = graph[e].is_trafo;
                pq.push({edge_weight[v], v});
            }
        }
    }
}

inline auto get_edge_weights(TransformerGraph const& graph) -> WeightedTrafoList {
    std::vector<EdgeWeight> edge_weight(boost::num_vertices(graph), infty);
    std::vector<Idx2D> edge_from_to(boost::num_vertices(graph));
    std::vector<bool> edge_is_trafo(boost::num_vertices(graph), false);

    BGL_FORALL_VERTICES(v, graph, TransformerGraph) {
        if (graph[v].is_source) {
            process_edges_dijkstra(v, edge_weight, edge_from_to, edge_is_trafo, graph);
        }
    }

    WeightedTrafoList result;
    for (size_t i = 0; i < edge_weight.size(); ++i) {
        const TrafoGraphEdge edge = {edge_is_trafo[i], edge_from_to[i], edge_weight[i]};
        result.push_back(edge);
    }

    return result;
}

inline auto rank_transformers(WeightedTrafoList const& w_trafo_list) -> RankedTransformerGroups {
    auto sorted_trafos = w_trafo_list;

    std::sort(sorted_trafos.begin(), sorted_trafos.end(),
              [](const TrafoGraphEdge& a, const TrafoGraphEdge& b) { return a.weight < b.weight; });

    RankedTransformerGroups groups;
    Idx last_weight = -1;
    for (const auto& trafo : sorted_trafos) {
        if (groups.empty() || last_weight != trafo.weight) {
            groups.push_back(std::vector<Idx2D>{trafo.from_to});
            last_weight = trafo.weight;
        } else {
            groups.back().push_back(trafo.from_to);
        }
    }
    return groups;
}

template <main_core::main_model_state_c State>
inline auto rank_transformers(State const& state) -> RankedTransformerGroups {
    return rank_transformers(get_edge_weights(build_transformer_graph(state)));
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

    TapPositionOptimizer(Calculator calculator, StateUpdater updater, OptimizerStrategy strategy)
        : calculate_{std::move(calculator)}, update_{std::move(updater)}, strategy_{strategy} {}

    auto optimize(State const& state) -> ResultType final {
        auto const order = rank_transformers(state);
        return optimize(state, order);
    }

    constexpr auto get_strategy() { return strategy_; }

  private:
    auto optimize(State const& /*state*/, RankedTransformerGroups const& /*order*/) -> ResultType {
        // TODO(mgovers): implement outter loop tap changer
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
