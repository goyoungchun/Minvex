#include "EditorUI.h"
#include "scene/ParticleGraph.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace lv::editor {
namespace {
struct Edits {
    bool Changed = false, Active = false;
    bool Track(bool changed) {
        Changed |= changed;
        Active |= ImGui::IsItemActive();
        return changed;
    }
};

ParticleEmitter Fountain() {
    ParticleEmitter emitter;
    emitter.Name = "Fountain";
    emitter.Blend = ParticleBlend::Additive;
    emitter.Spawn.Rate = 65;
    emitter.Spawn.Bursts = {{0, 16}};
    emitter.Shape = ParticleShape::Cone;
    emitter.Radius = 0.12f;
    emitter.RadialSpeed = 2.6f;
    emitter.Initial.Lifetime = {0.75f, 1.5f};
    emitter.Initial.Size = {0.07f, 0.16f};
    emitter.Acceleration = {0, -2.6f, 0};
    emitter.ColorOverLife = {{0, {0.4f, 0.85f, 1, 1}}, {0.65f, {0.14f, 0.47f, 1, 0.65f}}, {1, {0.15f, 0.2f, 1, 0}}};
    emitter.SizeOverLife = {{0, 1}, {1, 0.22f}};
    return emitter;
}

constexpr auto Clamp = ImGuiSliderFlags_AlwaysClamp;
constexpr float MaxValue = 1000000;
constexpr const char* EventNames[]{"Birth", "Collision", "Death"};
constexpr const char* NodeNames[]{"Constant", "Age", "Random", "Add", "Subtract", "Multiply", "Min", "Max", "Sine", "Saturate"};

void Drag(Edits& edits, const char* label, float& value, float minimum, float maximum, const char* format = "%.3f") {
    edits.Track(ImGui::DragFloat(label, &value, 0.02f, minimum, maximum, format, Clamp));
}

void Seconds(Edits& edits, const char* label, double& value, double minimum, double maximum) {
    edits.Track(ImGui::DragScalar(label, ImGuiDataType_Double, &value, 0.01f, &minimum, &maximum, "%.3f s", Clamp));
}

void DragVector(Edits& edits, const char* label, Vec3& value) {
    float items[]{value.X, value.Y, value.Z};
    if (edits.Track(ImGui::DragFloat3(label, items, 0.02f, -MaxValue, MaxValue, "%.3f", Clamp))) {
        value = {items[0], items[1], items[2]};
    }
}

void Range(Edits& edits, const char* label, ParticleRange<float>& value, float minimum, float maximum) {
    edits.Track(ImGui::DragFloatRange2(label, &value.Min, &value.Max, 0.02f, minimum, maximum, "%.3f", nullptr, Clamp));
}

void VectorRange(Edits& edits, const char* title, ParticleRange<Vec3>& range) {
    if (!ImGui::TreeNode(title)) { return; }
    DragVector(edits, "Minimum", range.Min);
    DragVector(edits, "Maximum", range.Max);
    const auto a = range.Min, b = range.Max;
    range.Min = {std::min(a.X, b.X), std::min(a.Y, b.Y), std::min(a.Z, b.Z)};
    range.Max = {std::max(a.X, b.X), std::max(a.Y, b.Y), std::max(a.Z, b.Z)};
    ImGui::TreePop();
}

void Spawn(Edits& edits, ParticleSpawnModule& spawn) {
    if (!ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) { return; }
    Drag(edits, "Rate", spawn.Rate, 0, 100000, "%.2f / s");
    Seconds(edits, "Delay", spawn.Delay, 0, MaxParticleStepSeconds);
    const double minDuration = spawn.Cycles != 1 ? 1.0 / 240 : 0;
    Seconds(edits, "Duration", spawn.Duration, minDuration, MaxParticleStepSeconds);
    if (spawn.Duration > 0 && spawn.Duration < 1.0 / 240 && spawn.Cycles != 1) { spawn.Duration = 1.0 / 240; }
    if (spawn.Duration == 0) { spawn.Cycles = 1; }
    else {
        edits.Track(ImGui::InputScalar("Cycles (0 = loop)", ImGuiDataType_U32, &spawn.Cycles));
        if (spawn.Cycles != 1) { spawn.Duration = std::max(spawn.Duration, 1.0 / 240); }
    }
    const double end = spawn.Duration > 0 ? spawn.Duration : MaxParticleStepSeconds;
    int remove = -1;
    for (std::size_t i = 0; i < spawn.Bursts.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto& burst = spawn.Bursts[i];
        ImGui::SeparatorText("Burst");
        Seconds(edits, "Time", burst.TimeSeconds, 0, end);
        burst.TimeSeconds = std::clamp(burst.TimeSeconds, 0.0, end);
        edits.Track(ImGui::InputScalar("Count", ImGuiDataType_U32, &burst.Count));
        burst.Count = std::max(burst.Count, 1u);
        if (ImGui::SmallButton("Remove burst")) { remove = static_cast<int>(i); }
        ImGui::PopID();
    }
    if (remove >= 0) { spawn.Bursts.erase(spawn.Bursts.begin() + remove); edits.Changed = true; }
    if (spawn.Bursts.size() < MaxParticleBursts && ImGui::Button("Add burst")) {
        spawn.Bursts.push_back({0, 16});
        edits.Changed = true;
    }
}

template<class T>
T CurveValue(const std::vector<ParticleKey<T>>& keys, float t) {
    const auto next = std::upper_bound(keys.begin(), keys.end(), t, [](float time, const auto& key) { return time < key.Time; });
    if (next == keys.begin()) { return next->Value; }
    const auto& a = *(next - 1);
    if (next == keys.end()) { return a.Value; }
    const auto mix = (t - a.Time) / (next->Time - a.Time);
    if constexpr (std::is_same_v<T, float>) { return std::lerp(a.Value, next->Value, mix); }
    else {
        T color{};
        for (std::size_t i = 0; i < color.size(); ++i) { color[i] = std::lerp(a.Value[i], next->Value[i], mix); }
        return color;
    }
}

template<class T>
void AddKey(std::vector<ParticleKey<T>>& keys, T initial) {
    if (keys.empty()) { keys.push_back({0, initial}); return; }
    if (keys.front().Time > 0) { keys.insert(keys.begin(), {0, keys.front().Value}); return; }
    if (keys.back().Time < 1) { keys.push_back({1, keys.back().Value}); return; }
    auto low = keys.begin();
    for (auto it = keys.begin(); it + 1 != keys.end(); ++it) {
        if ((it + 1)->Time - it->Time > (low + 1)->Time - low->Time) { low = it; }
    }
    const float midpoint = (low->Time + (low + 1)->Time) * 0.5f;
    const auto value = CurveValue(keys, midpoint);
    keys.insert(low + 1, {midpoint, value});
}

void CurvePreview(const std::vector<ParticleKey<float>>& keys) {
    std::array<float, 64> samples;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = CurveValue(keys, static_cast<float>(i) / static_cast<float>(samples.size() - 1));
    }
    ImGui::PlotLines("##Curve", samples.data(), static_cast<int>(samples.size()), 0, nullptr, 0,
        std::max(1.0f, *std::max_element(samples.begin(), samples.end())), {-1, 60});
}

void CurvePreview(const std::vector<ParticleKey<ParticleColor>>& keys) {
    const auto p = ImGui::GetCursorScreenPos();
    const auto width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy({width, 22});
    for (int i = 0; i < 64; ++i) {
        const auto c = CurveValue(keys, (static_cast<float>(i) + 0.5f) / 64);
        ImGui::GetWindowDrawList()->AddRectFilled({p.x + width * i / 64, p.y},
            {p.x + width * (i + 1) / 64 + 1, p.y + 22}, ImGui::GetColorU32({c[0], c[1], c[2], c[3]}));
    }
}

template<class T>
void Curve(Edits& edits, const char* title, std::vector<ParticleKey<T>>& keys, T initial) {
    if (!ImGui::CollapsingHeader(title)) { return; }
    ImGui::PushID(title);
    if (!keys.empty()) { CurvePreview(keys); }
    int remove = -1;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const auto lower = i ? std::nextafter(keys[i - 1].Time, 1.0f) : 0.0f;
        const auto upper = i + 1 < keys.size() ? std::nextafter(keys[i + 1].Time, 0.0f) : 1.0f;
        if (lower < upper) { Drag(edits, "Life fraction", keys[i].Time, lower, upper); }
        else { ImGui::Text("Life fraction %.3f", keys[i].Time); }
        if constexpr (std::is_same_v<T, float>) { Drag(edits, "Scale", keys[i].Value, 0, MaxValue); }
        else { edits.Track(ImGui::ColorEdit4("RGBA", keys[i].Value.data(), ImGuiColorEditFlags_AlphaBar)); }
        if (ImGui::SmallButton("Remove key")) { remove = static_cast<int>(i); }
        ImGui::PopID();
    }
    if (remove >= 0) { keys.erase(keys.begin() + remove); edits.Changed = true; }
    if (keys.size() < MaxParticleCurveKeys && ImGui::Button(keys.empty() ? "Add module" : "Add key")) {
        AddKey(keys, initial);
        edits.Changed = true;
    }
    if (!keys.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Remove module")) { keys.clear(); edits.Changed = true; }
    }
    ImGui::PopID();
}

void Modules(Edits& edits, ParticleEmitter& emitter) {
    if (ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
        edits.Track(ImGui::InputText("Name", &emitter.Name, ImGuiInputTextFlags_EnterReturnsTrue));
        if (emitter.Name.size() > 128) {
            std::size_t length = 128;
            while ((static_cast<unsigned char>(emitter.Name[length]) & 0xc0u) == 0x80u) { --length; }
            emitter.Name.resize(length);
        }
        edits.Track(ImGui::Checkbox("Enabled", &emitter.Enabled));
        int capacity = static_cast<int>(emitter.Capacity);
        if (edits.Track(ImGui::SliderInt("Capacity", &capacity, 1, static_cast<int>(MaxParticlesPerEmitter), "%d", Clamp))) {
            emitter.Capacity = static_cast<std::size_t>(capacity);
        }
        int space = static_cast<int>(emitter.Space);
        if (edits.Track(ImGui::Combo("Space", &space, "World\0Local\0"))) { emitter.Space = static_cast<ParticleSpace>(space); }
        int blend = static_cast<int>(emitter.Blend);
        if (edits.Track(ImGui::Combo("Blend", &blend, "Alpha\0Additive\0"))) { emitter.Blend = static_cast<ParticleBlend>(blend); }
        Drag(edits, "Aspect ratio", emitter.AspectRatio, 0.01f, 100);
        Drag(edits, "Max draw distance", emitter.MaxDrawDistance, 0, MaxValue);
    }
    Spawn(edits, emitter.Spawn);
    if (ImGui::CollapsingHeader("Initial values and shape")) {
        auto& initial = emitter.Initial;
        int shape = static_cast<int>(emitter.Shape);
        if (edits.Track(ImGui::Combo("Shape", &shape, "Box / point\0Sphere\0Disc\0Cone\0"))) {
            emitter.Shape = static_cast<ParticleShape>(shape);
        }
        if (emitter.Shape != ParticleShape::Box) { Drag(edits, "Radius", emitter.Radius, 0, MaxValue); }
        if (emitter.Shape == ParticleShape::Cone) { Drag(edits, "Cone half angle", emitter.ConeAngle, 0, 1.5707963f, "%.3f rad"); }
        Drag(edits, "Radial speed", emitter.RadialSpeed, -MaxValue, MaxValue);
        VectorRange(edits, "Position", initial.Position);
        VectorRange(edits, "Velocity", initial.Velocity);
        Range(edits, "Lifetime", initial.Lifetime, 0.001f, 3600);
        Range(edits, "Size", initial.Size, 0, MaxValue);
        Range(edits, "Rotation (rad)", initial.Rotation, -MaxValue, MaxValue);
        Range(edits, "Spin (rad/s)", initial.AngularVelocity, -MaxValue, MaxValue);
    }
    if (ImGui::CollapsingHeader("Forces")) {
        DragVector(edits, "Acceleration", emitter.Acceleration);
        Drag(edits, "Linear drag", emitter.Drag, 0, MaxValue);
        if (ImGui::Button("Reset forces")) { emitter.Acceleration = {}; emitter.Drag = 0; edits.Changed = true; }
    }
    if (ImGui::CollapsingHeader("Collision")) {
        auto& collision = emitter.Collision;
        int mode = static_cast<int>(collision.Mode);
        if (edits.Track(ImGui::Combo("Response", &mode, "Off\0Kill\0Bounce\0"))) {
            collision.Mode = static_cast<ParticleCollision>(mode);
        }
        if (collision.Mode != ParticleCollision::Off) {
            int source = static_cast<int>(collision.Source);
            if (edits.Track(ImGui::Combo("Source", &source, "Plane\0Terrain\0Rigid meshes\0Terrain and meshes\0"))) {
                collision.Source = static_cast<ParticleColliderSource>(source);
            }
            if (collision.Source == ParticleColliderSource::Plane) { Drag(edits, "Plane Y", collision.Height, -MaxValue, MaxValue); }
            if (collision.Mode == ParticleCollision::Bounce) { Drag(edits, "Restitution", collision.Restitution, 0, 1); }
        }
    }
    if (ImGui::CollapsingHeader("Trail")) {
        auto& trail = emitter.Trail;
        Drag(edits, "Length (seconds)", trail.Seconds, 0, 10);
        if (trail.Seconds > 0) {
            Drag(edits, "Width multiplier", trail.Width, 0, 100);
            int segments = trail.Segments;
            if (edits.Track(ImGui::SliderInt("Segments", &segments, 1, 8, "%d", Clamp))) {
                trail.Segments = static_cast<std::uint16_t>(segments);
            }
        }
    }
    Curve(edits, "Color over life", emitter.ColorOverLife, ParticleColor{1, 1, 1, 1});
    Curve(edits, "Size over life", emitter.SizeOverLife, 1.0f);
}

std::string EmitterName(const std::vector<ParticleEmitter>& emitters, std::size_t index) {
    return emitters[index].Name.empty() ? "Emitter " + std::to_string(index + 1) : emitters[index].Name;
}

bool ReachesEmitter(const std::vector<ParticleEmitter>& emitters, std::size_t start, std::size_t target) {
    std::array<bool, MaxParticleEmittersPerEffect> visited{};
    const auto visit = [&](auto&& self, std::size_t i) -> bool {
        if (i == target) { return true; }
        if (visited[i]) { return false; }
        visited[i] = true;
        for (const auto& route : emitters[i].EventRoutes) { if (self(self, route.Target)) { return true; } }
        return false;
    };
    return visit(visit, start);
}

void EventModule(Edits& edits, std::vector<ParticleEmitter>& emitters, std::size_t i) {
    if (!ImGui::CollapsingHeader("Events and sub-emitters")) { return; }
    auto& emitter = emitters[i];
    edits.Track(ImGui::Checkbox("Publish events", &emitter.EmitEvents));
    int remove = -1;
    for (std::size_t j = 0; j < emitter.EventRoutes.size(); ++j) {
        ImGui::PushID(static_cast<int>(j));
        auto& route = emitter.EventRoutes[j];
        int kind = static_cast<int>(route.Event);
        if (edits.Track(ImGui::Combo("Event", &kind, EventNames, 3))) { route.Event = static_cast<ParticleEventKind>(kind); }
        if (ImGui::BeginCombo("Spawn emitter", EmitterName(emitters, route.Target).c_str())) {
            for (std::size_t k = 0; k < emitters.size(); ++k) {
                if (ReachesEmitter(emitters, k, i)) { continue; }
                ImGui::PushID(static_cast<int>(k));
                if (ImGui::Selectable(EmitterName(emitters, k).c_str(), k == route.Target)) {
                    route.Target = static_cast<std::uint8_t>(k);
                    edits.Changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        int count = route.Count;
        if (edits.Track(ImGui::DragInt("Spawn count", &count, 1, 1, static_cast<int>(MaxParticlesPerEmitter), "%d", Clamp))) {
            route.Count = static_cast<std::uint16_t>(count);
        }
        if (ImGui::SmallButton("Remove route")) { remove = static_cast<int>(j); }
        ImGui::PopID();
    }
    if (remove >= 0) { emitter.EventRoutes.erase(emitter.EventRoutes.begin() + remove); edits.Changed = true; }
    std::size_t target = 0;
    while (target < emitters.size() && ReachesEmitter(emitters, target, i)) { ++target; }
    ImGui::BeginDisabled(target == emitters.size() || emitter.EventRoutes.size() == MaxParticleEventRoutes);
    if (ImGui::Button("Add sub-emitter route")) {
        emitter.EventRoutes.push_back({ParticleEventKind::Death, static_cast<std::uint8_t>(target), 1});
        edits.Changed = true;
    }
    ImGui::EndDisabled();
}

std::string NodeName(const ParticleModuleGraph& graph, std::uint8_t index) {
    return index == NoParticleNode ? "Unconnected" :
        std::to_string(index + 1) + " " + NodeNames[static_cast<unsigned>(graph.Nodes[index].Op)];
}

bool ReachesNode(const ParticleModuleGraph& graph, std::uint8_t start, std::size_t target) {
    std::array<bool, MaxParticleGraphNodes> visited{};
    const auto visit = [&](auto&& self, std::uint8_t i) -> bool {
        if (i >= graph.Nodes.size()) { return false; }
        if (i == target) { return true; }
        if (visited[i]) { return false; }
        visited[i] = true;
        const auto& node = graph.Nodes[i];
        const auto inputs = detail::GraphInputs(node.Op);
        return (inputs >= 1 && self(self, node.A)) || (inputs == 2 && self(self, node.B));
    };
    return visit(visit, start);
}

void GraphLink(Edits& edits, const char* title, ParticleModuleGraph& graph, std::uint8_t& link, std::size_t node, bool output) {
    if (!ImGui::BeginCombo(title, NodeName(graph, link).c_str())) { return; }
    if (output && ImGui::Selectable("Unconnected", link == NoParticleNode)) { link = NoParticleNode; edits.Changed = true; }
    for (std::uint8_t i = 0; i < graph.Nodes.size(); ++i) {
        if (ReachesNode(graph, i, node)) { continue; }
        if (ImGui::Selectable(NodeName(graph, i).c_str(), link == i)) { link = i; edits.Changed = true; }
    }
    ImGui::EndCombo();
}

void GraphCanvas(const ParticleModuleGraph& graph) {
    if (graph.Nodes.empty()) { return; }
    const auto origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    if (width < 100) { return; }
    const float cardWidth = std::min(132.0f, width * 0.45f);
    const auto position = [&](std::size_t i) {
        return ImVec2{origin.x + (i % 2 ? width - cardWidth : 0), origin.y + 32.0f * static_cast<float>(i / 2)};
    };
    auto* draw = ImGui::GetWindowDrawList();
    for (std::size_t i = 0; i < graph.Nodes.size(); ++i) {
        const auto& node = graph.Nodes[i];
        const auto end = position(i);
        const auto link = [&](std::uint8_t from) {
            if (from == NoParticleNode) { return; }
            const auto start = position(from);
            draw->AddLine({start.x + cardWidth * 0.5f,start.y + 12}, {end.x + cardWidth * 0.5f,end.y + 12},
                          ImGui::GetColorU32(ImGuiCol_PlotLines), 1.5f);
        };
        const auto inputs = detail::GraphInputs(node.Op);
        if (inputs >= 1) { link(node.A); }
        if (inputs == 2) { link(node.B); }
    }
    for (std::uint8_t i = 0; i < graph.Nodes.size(); ++i) {
        const auto p = position(i);
        draw->AddRectFilled(p, {p.x + cardWidth,p.y + 24}, ImGui::GetColorU32(ImGuiCol_FrameBg), 3);
        draw->AddText({p.x + 6,p.y + 3}, ImGui::GetColorU32(ImGuiCol_Text), NodeName(graph, i).c_str());
    }
    ImGui::Dummy({width, 32.0f * static_cast<float>((graph.Nodes.size() + 1) / 2)});
}

void GraphModule(Edits& edits, ParticleModuleGraph& graph) {
    if (!ImGui::CollapsingHeader("Module graph")) { return; }
    GraphCanvas(graph);
    GraphLink(edits, "Size multiplier", graph, graph.Size, graph.Nodes.size(), true);
    GraphLink(edits, "Opacity multiplier", graph, graph.Opacity, graph.Nodes.size(), true);
    GraphLink(edits, "Rotation offset", graph, graph.Rotation, graph.Nodes.size(), true);
    GraphLink(edits, "Initial speed multiplier", graph, graph.Speed, graph.Nodes.size(), true);
    int remove = -1;
    for (std::size_t i = 0; i < graph.Nodes.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto& node = graph.Nodes[i];
        if (ImGui::TreeNode("##Node", "%s", NodeName(graph, static_cast<std::uint8_t>(i)).c_str())) {
            auto source = NoParticleNode;
            for (std::uint8_t k = 0; k < graph.Nodes.size(); ++k) {
                if (!ReachesNode(graph, k, i)) { source = k; break; }
            }
            if (ImGui::BeginCombo("Operation", NodeNames[static_cast<unsigned>(node.Op)])) {
                for (unsigned op = 0; op < std::size(NodeNames); ++op) {
                    const auto candidate = static_cast<ParticleNodeOp>(op);
                    const auto inputs = detail::GraphInputs(candidate);
                    if (inputs && source == NoParticleNode) { continue; }
                    if (ImGui::Selectable(NodeNames[op], node.Op == candidate) && node.Op != candidate) {
                        node.Op = candidate;
                        if (inputs && (node.A >= graph.Nodes.size() || ReachesNode(graph, node.A, i))) { node.A = source; }
                        if (inputs == 2 && (node.B >= graph.Nodes.size() || ReachesNode(graph, node.B, i))) { node.B = source; }
                        edits.Changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (node.Op == ParticleNodeOp::Constant) { Drag(edits, "Value", node.Value, -MaxValue, MaxValue); }
            const auto inputs = detail::GraphInputs(node.Op);
            if (inputs) { GraphLink(edits, "Input A", graph, node.A, i, false); }
            if (inputs == 2) { GraphLink(edits, "Input B", graph, node.B, i, false); }
            if (ImGui::SmallButton("Remove node")) { remove = static_cast<int>(i); }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (remove >= 0) {
        graph.Nodes.erase(graph.Nodes.begin() + remove);
        const auto remap = [&](std::uint8_t& link) {
            if (link == remove) { link = NoParticleNode; }
            else if (link != NoParticleNode && link > remove) { --link; }
        };
        for (auto& node : graph.Nodes) {
            const auto inputs = detail::GraphInputs(node.Op);
            remap(node.A);
            remap(node.B);
            if ((inputs && node.A == NoParticleNode) || (inputs == 2 && node.B == NoParticleNode)) { node = {}; }
        }
        for (auto* link : {&graph.Size, &graph.Opacity, &graph.Rotation, &graph.Speed}) { remap(*link); }
        edits.Changed = true;
    }
    if (graph.Nodes.size() < MaxParticleGraphNodes && ImGui::Button("Add node")) {
        graph.Nodes.push_back({});
        edits.Changed = true;
    }
}

void RendererModule(Edits& edits, ParticleEmitter& emitter, const std::vector<ModelHandle>& assets) {
    if (!ImGui::CollapsingHeader("Renderer and simulation")) { return; }
    int renderer = static_cast<int>(emitter.Geometry.Type);
    if (edits.Track(ImGui::Combo("Renderer", &renderer, "Sprite\0Mesh\0Ribbon\0Beam\0"))) {
        emitter.Geometry.Type = static_cast<ParticleRendererType>(renderer);
    }
    if (emitter.Geometry.Type == ParticleRendererType::Mesh) {
        const auto name = [](const ModelHandle& model) {
            if (!model) { return std::string("Unit cube"); }
            const auto text = model->SourcePath.filename().u8string();
            return std::string(text.begin(), text.end());
        };
        if (ImGui::BeginCombo("Mesh asset", name(emitter.Geometry.Mesh).c_str())) {
            if (ImGui::Selectable("Unit cube", !emitter.Geometry.Mesh)) { emitter.Geometry.Mesh.reset(); edits.Changed = true; }
            for (const auto& asset : assets) {
                ImGui::PushID(asset.get());
                if (ImGui::Selectable(name(asset).c_str(), emitter.Geometry.Mesh == asset)) {
                    emitter.Geometry.Mesh = asset;
                    edits.Changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    } else if (emitter.Geometry.Type == ParticleRendererType::Beam) {
        DragVector(edits, "Endpoint offset", emitter.Geometry.BeamTarget);
    }
    int simulation = static_cast<int>(emitter.Simulation);
    if (edits.Track(ImGui::Combo("Simulation", &simulation, "CPU\0GPU\0"))) {
        emitter.Simulation = static_cast<ParticleSimulation>(simulation);
    }
    if (emitter.Simulation == ParticleSimulation::Gpu) {
        const bool compatible = emitter.Geometry.Type == ParticleRendererType::Sprite && emitter.Blend == ParticleBlend::Additive &&
            emitter.Collision.Mode == ParticleCollision::Off && !emitter.Trail.Seconds && !emitter.Flipbook.Texture;
        ImGui::TextWrapped("%s", compatible ? "GPU motion on compute-capable devices." :
            "This configuration previews on the CPU. GPU motion supports untextured Additive sprites without collision or trails.");
    }
}

void FlipbookGrid(Edits& edits, ParticleFlipbook& flipbook) {
    const auto grid = [&](const char* label, std::uint16_t& value, int dimension) {
        int choice = value;
        if (ImGui::BeginCombo(label, std::to_string(choice).c_str())) {
            for (int n = 1; n <= 64 && n <= dimension; ++n) {
                if (dimension % n == 0 && ImGui::Selectable(std::to_string(n).c_str(), n == choice)) {
                    value = static_cast<std::uint16_t>(n);
                    edits.Changed = true;
                }
            }
            ImGui::EndCombo();
        }
    };
    grid("Columns", flipbook.Columns, flipbook.Texture->Width());
    grid("Rows", flipbook.Rows, flipbook.Texture->Height());
    int cycles = flipbook.Cycles;
    if (edits.Track(ImGui::SliderInt("Cycles per lifetime", &cycles, 1, 64, "%d", Clamp))) {
        flipbook.Cycles = static_cast<std::uint16_t>(cycles);
    }
}
} // namespace

void EditorUI::TickParticlePreview(double delta) {
    if (particlePlaying_ && Document.GetScene().HasParticleEffects()) {
        if (!Report(Document.UpdateParticles(delta * particleSpeed_))) { particlePlaying_ = false; }
    }
    auto events = Document.PollParticleEvents();
    if (!events || events.Value().Events.empty()) { return; }
    const auto& batch = events.Value();
    particleDroppedEvents_ += batch.Dropped;
    constexpr std::size_t maxHistory = 128;
    const auto take = std::min(maxHistory, batch.Events.size());
    if (particleEvents_.size() + take > maxHistory) {
        particleEvents_.erase(particleEvents_.begin(), particleEvents_.begin() +
            static_cast<std::ptrdiff_t>(particleEvents_.size() + take - maxHistory));
    }
    particleEvents_.insert(particleEvents_.end(), batch.Events.end() - static_cast<std::ptrdiff_t>(take), batch.Events.end());
}

void EditorUI::FinishParticleEdit(bool cancel) {
    if (particleEdit_ && Document.IsEditing()) { Report(cancel ? Document.CancelEdit() : Document.CommitEdit()); }
    particleEdit_ = false;
}

void EditorUI::ParticlePanel() {
    if (!particlePanel_) { FinishParticleEdit(); return; }
    if (particleFocusRequested_) { ImGui::SetNextWindowFocus(); particleFocusRequested_ = false; }
    if (!ImGui::Begin("Particles", &particlePanel_)) { FinishParticleEdit(); ImGui::End(); return; }
    const auto object = Document.Selection();
    if (particleObject_ != object) {
        FinishParticleEdit();
        particleObject_ = object;
        selectedEmitter_ = 0;
    }
    if (particleEdit_ && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { FinishParticleEdit(true); ImGui::End(); return; }
    const auto binding = object ? Document.GetScene().GetParticleEffect(object) : ParticleEffectBinding{};
    if (ImGui::Button("New particle object")) { FinishParticleEdit(); Report(Document.CreateParticleObject(MakeParticleEffect({Fountain()}))); }
    if (!binding) {
        if (object && ImGui::Button("Attach effect to selection")) { Report(Document.SetParticleEffect(object, MakeParticleEffect({Fountain()}))); }
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Play scene particles", &particlePlaying_);
    ImGui::SameLine();
    if (ImGui::SmallButton("Restart")) { Report(Document.SeekParticles(object, 0), object); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Step")) { particlePlaying_ = false; Report(Document.UpdateParticles(1.0 / 60), object); }
    if (ImGui::SmallButton("Frame emitter")) {
        auto camera = Document.GetScene().MainCamera;
        const auto p = Document.GetScene().GetWorldTransform(object).TransformPoint({});
        camera.Target = {p.X, p.Y + 0.6f, p.Z};
        camera.Position = {p.X + 2, p.Y + 1.4f, p.Z + 4};
        Report(Document.SetCamera(camera), object);
    }
    ImGui::SliderFloat("Speed", &particleSpeed_, 0.1f, 4, "%.2fx", Clamp);
    float time = static_cast<float>(Document.GetScene().ParticleTime(object));
    float duration = 5;
    for (const auto& emitter : binding.Effect->Emitters()) {
        const auto end = emitter.Spawn.Duration * (emitter.Spawn.Cycles ? emitter.Spawn.Cycles : 1);
        duration = std::max(duration, static_cast<float>(std::min(MaxParticleStepSeconds,
            emitter.Spawn.Delay + end + emitter.Initial.Lifetime.Max)));
    }
    if (ImGui::SliderFloat("Preview time", &time, 0, std::max(time, duration), "%.3f s", Clamp)) {
        particlePlaying_ = false;
        Report(Document.SeekParticles(object, time), object);
    }
    ImGui::Text("%zu live particles", Document.GetScene().ParticleCount(object));
    auto seed = binding.Seed;
    Edits edits;
    edits.Track(ImGui::InputScalar("Seed", ImGuiDataType_U32, &seed));
    bool detach = ImGui::SmallButton("Detach effect");
    ImGui::SeparatorText("Emitters");

    auto emitters = binding.Effect->Emitters();
    selectedEmitter_ = std::min(selectedEmitter_, emitters.size() - 1);
    if (ImGui::BeginListBox("##Emitters", {-1, ImGui::GetTextLineHeightWithSpacing() * 4.5f})) {
        for (std::size_t i = 0; i < emitters.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto& emitter = emitters[i];
            edits.Changed |= ImGui::Checkbox("##Enabled", &emitter.Enabled);
            ImGui::SameLine();
            const auto label = (emitter.Name.empty() ? "Emitter " + std::to_string(i + 1) : emitter.Name) + "###Emitter";
            if (ImGui::Selectable(label.c_str(), selectedEmitter_ == i)) { FinishParticleEdit(); selectedEmitter_ = i; }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::BeginDisabled(emitters.size() >= MaxParticleEmittersPerEffect);
    if (ImGui::SmallButton("Add")) { emitters.push_back(Fountain()); selectedEmitter_ = emitters.size() - 1; edits.Changed = true; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Duplicate")) {
        auto copy = emitters[selectedEmitter_];
        emitters.insert(emitters.begin() + static_cast<std::ptrdiff_t>(++selectedEmitter_), std::move(copy));
        for (auto& emitter : emitters) { for (auto& route : emitter.EventRoutes) { if (route.Target >= selectedEmitter_) { ++route.Target; } } }
        edits.Changed = true;
    }
    ImGui::EndDisabled();
    const auto move = [&](int direction) {
        const auto next = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(selectedEmitter_) + direction);
        if (next >= emitters.size()) { return; }
        std::swap(emitters[selectedEmitter_], emitters[next]);
        for (auto& emitter : emitters) {
            for (auto& route : emitter.EventRoutes) {
                if (route.Target == selectedEmitter_) { route.Target = static_cast<std::uint8_t>(next); }
                else if (route.Target == next) { route.Target = static_cast<std::uint8_t>(selectedEmitter_); }
            }
        }
        selectedEmitter_ = next;
        edits.Changed = true;
    };
    ImGui::SameLine(); if (ImGui::SmallButton("Up")) { move(-1); }
    ImGui::SameLine(); if (ImGui::SmallButton("Down")) { move(1); }
    ImGui::SameLine(); if (ImGui::SmallButton("Remove")) {
        emitters.erase(emitters.begin() + static_cast<std::ptrdiff_t>(selectedEmitter_));
        for (auto& emitter : emitters) {
            std::erase_if(emitter.EventRoutes, [&](const auto& route) { return route.Target == selectedEmitter_; });
            for (auto& route : emitter.EventRoutes) { if (route.Target > selectedEmitter_) { --route.Target; } }
        }
        detach = emitters.empty();
        if (!detach) { selectedEmitter_ = std::min(selectedEmitter_, emitters.size() - 1); }
        edits.Changed = true;
    }
    if (!detach) {
        const auto selected = static_cast<int>(selectedEmitter_);
        ImGui::PushID(selected);
        auto& emitter = emitters[selectedEmitter_];
        Modules(edits, emitter);
        EventModule(edits, emitters, selectedEmitter_);
        GraphModule(edits, emitter.Graph);
        RendererModule(edits, emitter, Document.Assets());
        if (ImGui::CollapsingHeader("Sprite texture and flipbook")) {
            if (ImGui::Button("Choose PNG")) {
                const FilePickerRequest request{FilePickerMode::Open, FilePickerFilter::Png, directory_, {}, "Choose a particle sprite sheet"};
                const auto chosen = picker_ ? picker_(request) : nativeFiles_.Choose(request);
                controls_.Reset();
                if (Report(chosen, object) && chosen.Value() && !chosen.Value()->empty()) {
                    const auto& path = chosen.Value()->front();
                    try {
                        emitter.Flipbook = {LoadParticleTexture(path)};
                        edits.Changed = true;
                    } catch (const std::exception& error) {
                        ShowError({ErrorCode::InvalidData, error.what(), "Choose a PNG sprite sheet up to 2048 pixels per axis.", path}, object);
                    }
                }
            }
            if (emitter.Flipbook.Texture) {
                const auto name = emitter.Flipbook.Texture->Path().filename().u8string();
                ImGui::TextUnformatted(reinterpret_cast<const char*>(name.c_str()));
                FlipbookGrid(edits, emitter.Flipbook);
                if (ImGui::Button("Remove texture")) { emitter.Flipbook = {}; edits.Changed = true; }
            } else { ImGui::TextUnformatted("Procedural sprite"); }
        }
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Event log")) {
        if (ImGui::SmallButton("Clear log")) { particleEvents_.clear(); particleDroppedEvents_ = 0; }
        if (particleDroppedEvents_) { ImGui::Text("%zu events dropped", particleDroppedEvents_); }
        for (const auto& event : particleEvents_) {
            if (event.Object != object) { continue; }
            ImGui::Text("%.3f s  %s  Emitter %zu  #%u", event.Time, EventNames[static_cast<unsigned>(event.Kind)],
                event.Emitter + 1, event.Sequence);
        }
    }
    if (detach || edits.Changed) {
        if (edits.Active && !Document.IsEditing()) { particleEdit_ = Report(Document.BeginEdit()); }
        if (!Document.IsEditing() || particleEdit_) {
            try {
                if (!Report(Document.SetParticleEffect(object, detach ? ParticleEffectHandle{} :
                    MakeParticleEffect(std::move(emitters)), seed), object)) { FinishParticleEdit(true); }
            } catch (const std::exception& error) {
                FinishParticleEdit(true);
                ShowError({ErrorCode::InvalidArgument, error.what(), "Check emitter limits and curve values.", {}}, object);
            }
        }
    }
    if (!edits.Active) { FinishParticleEdit(); }
    ImGui::End();
}
} // namespace lv::editor
