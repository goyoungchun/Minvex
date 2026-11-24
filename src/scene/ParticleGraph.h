#pragma once

#include <minvex/Particles.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace lv::detail {

inline unsigned GraphInputs(ParticleNodeOp op) {
    switch (op) {
    case ParticleNodeOp::Constant:
    case ParticleNodeOp::Age:
    case ParticleNodeOp::Random: return 0;
    case ParticleNodeOp::Sine:
    case ParticleNodeOp::Saturate: return 1;
    case ParticleNodeOp::Add:
    case ParticleNodeOp::Subtract:
    case ParticleNodeOp::Multiply:
    case ParticleNodeOp::Min:
    case ParticleNodeOp::Max: return 2;
    }
    throw std::invalid_argument("Unknown particle graph operation.");
}

inline std::vector<std::uint8_t> CompileGraph(const ParticleModuleGraph& graph) {
    if (graph.Nodes.size() > MaxParticleGraphNodes) { throw std::length_error("Too many particle graph nodes."); }
    std::vector<std::uint8_t> order;
    std::array<std::uint8_t, MaxParticleGraphNodes> state{};
    const auto visit = [&](auto&& self, std::uint8_t index) -> void {
        if (index >= graph.Nodes.size()) { throw std::invalid_argument("Invalid particle graph connection."); }
        auto& mark = state[index];
        if (mark == 1) { throw std::invalid_argument("Particle graph contains a cycle."); }
        if (mark == 2) { return; }
        mark = 1;
        const auto& node = graph.Nodes[index];
        if (!std::isfinite(node.Value) || std::abs(node.Value) > 1e6f) {
            throw std::invalid_argument("Invalid particle graph constant.");
        }
        const auto count = GraphInputs(node.Op);
        if (count >= 1) { self(self, node.A); }
        if (count == 2) { self(self, node.B); }
        mark = 2;
        order.push_back(index);
    };
    for (const auto output : {graph.Size, graph.Opacity, graph.Rotation, graph.Speed}) {
        if (output != NoParticleNode) { visit(visit, output); }
    }
    const auto connected = order.size();
    for (std::uint8_t index = 0; index < graph.Nodes.size(); ++index) { visit(visit, index); }
    order.resize(connected);
    return order;
}

struct GraphValues {
    float Size = 1, Opacity = 1, Rotation = 0, Speed = 1;
};

inline GraphValues EvaluateGraph(const ParticleModuleGraph& graph, const std::vector<std::uint8_t>& order, float age, float random) {
    if (order.empty()) { return {}; }
    std::array<float, MaxParticleGraphNodes> values{};
    for (auto index : order) {
        const auto& node = graph.Nodes[index];
        const auto a = node.A < values.size() ? values[node.A] : 0;
        const auto b = node.B < values.size() ? values[node.B] : 0;
        float result = 0;
        switch (node.Op) {
        case ParticleNodeOp::Constant: result = node.Value; break;
        case ParticleNodeOp::Age: result = age; break;
        case ParticleNodeOp::Random: result = random; break;
        case ParticleNodeOp::Add: result = a + b; break;
        case ParticleNodeOp::Subtract: result = a - b; break;
        case ParticleNodeOp::Multiply: result = a * b; break;
        case ParticleNodeOp::Min: result = std::min(a, b); break;
        case ParticleNodeOp::Max: result = std::max(a, b); break;
        case ParticleNodeOp::Sine: result = std::sin(a); break;
        case ParticleNodeOp::Saturate: result = std::clamp(a, 0.0f, 1.0f); break;
        }
        values[index] = std::clamp(result, -1e6f, 1e6f);
    }
    const auto get = [&](std::uint8_t index, float defaultValue) { return index == NoParticleNode ? defaultValue : values[index]; };
    return {std::clamp(get(graph.Size, 1), 0.0f, 100.0f), std::clamp(get(graph.Opacity, 1), 0.0f, 1.0f),
            get(graph.Rotation, 0), std::clamp(get(graph.Speed, 1), -100.0f, 100.0f)};
}
} // namespace lv::detail
