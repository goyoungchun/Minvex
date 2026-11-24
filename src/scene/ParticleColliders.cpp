#include "ParticleColliders.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace lv::detail {

Vec3 ParticleAdd(Vec3 a, Vec3 b) noexcept { return {a.X + b.X, a.Y + b.Y, a.Z + b.Z}; }
Vec3 ParticleSubtract(Vec3 a, Vec3 b) noexcept { return {a.X - b.X, a.Y - b.Y, a.Z - b.Z}; }
Vec3 ParticleScale(Vec3 a, float scale) noexcept { return {a.X * scale, a.Y * scale, a.Z * scale}; }
float ParticleDot(Vec3 a, Vec3 b) noexcept { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
unsigned ParticleColliderMask(ParticleColliderSource source) noexcept {
    switch (source) {
    case ParticleColliderSource::Terrain: return 1;
    case ParticleColliderSource::Mesh: return 2;
    case ParticleColliderSource::Scene: return 3;
    default: return 0;
    }
}

namespace {
constexpr std::size_t MaxTriangles = 1048576;
float Axis(Vec3 v, unsigned i) { return i == 0 ? v.X : i == 1 ? v.Y : v.Z; }
Vec3 Cross(Vec3 a, Vec3 b) { return {a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X}; }
Vec3 Min(Vec3 a, Vec3 b) { return {std::min(a.X,b.X), std::min(a.Y,b.Y), std::min(a.Z,b.Z)}; }
Vec3 Max(Vec3 a, Vec3 b) { return {std::max(a.X,b.X), std::max(a.Y,b.Y), std::max(a.Z,b.Z)}; }
bool ValidPoint(Vec3 p) { return std::abs(p.X) <= 1e8f && std::abs(p.Y) <= 1e8f && std::abs(p.Z) <= 1e8f; }

bool Intersects(Bounds box, Vec3 start, Vec3 direction, float last) {
    float first = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const auto origin = Axis(start, i), delta = Axis(direction, i);
        const auto lower = Axis(box.Min, i), upper = Axis(box.Max, i);
        if (delta == 0) {
            if (origin < lower || origin > upper) { return false; }
            continue;
        }
        auto a = (lower - origin) / delta, b = (upper - origin) / delta;
        if (a > b) { std::swap(a, b); }
        first = std::max(first, a);
        last = std::min(last, b);
        if (last < first) { return false; }
    }
    return true;
}
} // namespace

void ParticleColliders::Prepare(const Scene& scene, unsigned mask) {
    auto items = scene.CollectRenderItems();
    std::uint64_t signature = (14695981039346656037ull ^ mask) * 1099511628211ull;
    const auto mix = [&](std::uint64_t value) { signature = (signature ^ value) * 1099511628211ull; };
    const auto world = [](const RenderItem& item, const ModelPart& part) {
        return item.WorldTransform * (item.Pose && part.Node != NoJoint ? item.Pose->Global.at(part.Node) : part.Transform);
    };
    for (const auto& item : items) {
        if (!(mask & (item.Model->Terrain ? 1u : 2u))) { continue; }
        mix(item.Object);
        mix(reinterpret_cast<std::uintptr_t>(item.Model.get()));
        for (const auto& part : item.Model->Parts) {
            if (part.SkinIndex) { continue; }
            for (auto value : world(item, part).Elements) { mix(std::bit_cast<std::uint32_t>(value)); }
        }
    }
    if (signature_ == signature) { return; }

    ParticleColliders next;
    for (const auto& item : items) {
        const unsigned category = item.Model->Terrain ? 1u : 2u;
        if (!(mask & category)) { continue; }
        next.models_.push_back(item.Model);
        for (const auto& part : item.Model->Parts) {
            if (part.SkinIndex) { continue; }
            const auto transform = world(item, part);
            for (const auto& primitive : item.Model->Meshes.at(part.MeshIndex).Primitives) {
                const auto& mesh = primitive.Geometry;
                if (mesh.Indices.size() / 3 > MaxTriangles - next.triangles_.size()) {
                    throw std::length_error("Particle colliders exceed 1,048,576 rigid triangles.");
                }
                std::vector<Vec3> points;
                points.reserve(mesh.Vertices.size());
                for (const auto& vertex : mesh.Vertices) {
                    points.push_back(transform.TransformPoint({vertex.Position[0], vertex.Position[1], vertex.Position[2]}));
                }
                for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3) {
                    const auto a = points.at(mesh.Indices[i]), b = points.at(mesh.Indices[i + 1]), c = points.at(mesh.Indices[i + 2]);
                    if (!ValidPoint(a) || !ValidPoint(b) || !ValidPoint(c)) { continue; }
                    next.triangles_.push_back({a, b, c, {Min(a, Min(b,c)), Max(a, Max(b,c))}, category});
                }
            }
        }
    }
    if (!next.triangles_.empty()) { next.Build(0, next.triangles_.size()); }
    next.signature_ = signature;
    *this = std::move(next);
}

std::size_t ParticleColliders::Build(std::size_t first, std::size_t last) {
    Bounds bounds = triangles_[first].Box;
    unsigned mask = 0;
    for (std::size_t i = first; i < last; ++i) {
        bounds.Min = Min(bounds.Min, triangles_[i].Box.Min);
        bounds.Max = Max(bounds.Max, triangles_[i].Box.Max);
        mask |= triangles_[i].Mask;
    }
    const auto index = nodes_.size();
    nodes_.push_back({bounds, first, last - first, 0, mask});
    if (last - first <= 8) { return index; }
    const auto extent = ParticleSubtract(bounds.Max, bounds.Min);
    const unsigned axis = extent.X >= extent.Y && extent.X >= extent.Z ? 0 : extent.Y >= extent.Z ? 1 : 2;
    const auto split = first + (last - first) / 2;
    std::nth_element(triangles_.begin() + first, triangles_.begin() + split, triangles_.begin() + last,
        [axis](const Triangle& a, const Triangle& b) {
            return Axis(a.Box.Min,axis) + Axis(a.Box.Max,axis) < Axis(b.Box.Min,axis) + Axis(b.Box.Max,axis);
        });
    Build(first, split);
    const auto right = Build(split, last);
    nodes_[index].Right = right;
    nodes_[index].Count = 0;
    return index;
}

std::optional<ParticleHit> ParticleColliders::Trace(Vec3 start, Vec3 end, ParticleColliderSource source) const noexcept {
    if (nodes_.empty() || !ValidPoint(start) || !ValidPoint(end)) { return {}; }
    const auto direction = ParticleSubtract(end, start);
    const auto mask = ParticleColliderMask(source);
    float closest = 1;
    std::optional<ParticleHit> hit;
    const auto visit = [&](auto&& self, std::size_t index) -> void {
        const auto& node = nodes_[index];
        if (!(node.Mask & mask) || !Intersects(node.Box, start, direction, closest)) { return; }
        if (!node.Count) { self(self, index + 1); self(self, node.Right); return; }
        for (std::size_t i = node.First; i < node.First + node.Count; ++i) {
            const auto& tri = triangles_[i];
            if (!(tri.Mask & mask)) { continue; }
            const auto ab = ParticleSubtract(tri.B, tri.A), ac = ParticleSubtract(tri.C, tri.A);
            const auto p = Cross(direction, ac);
            const auto determinant = ParticleDot(ab, p);
            if (std::abs(determinant) <= 1e-15f) { continue; }
            const auto delta = ParticleSubtract(start, tri.A), q = Cross(delta, ab);
            const auto u = ParticleDot(delta, p) / determinant, v = ParticleDot(direction, q) / determinant;
            const auto t = ParticleDot(ac, q) / determinant;
            if (u < -1e-6f || v < -1e-6f || u + v > 1.000001f || t < 0 || t > closest) { continue; }
            const auto n = Cross(ab, ac);
            const auto length = std::hypot(n.X, n.Y, n.Z);
            if (!length) { continue; }
            closest = t;
            hit = ParticleHit{t, ParticleScale(n, (ParticleDot(direction,n) > 0 ? -1.0f : 1.0f) / length)};
        }
    };
    visit(visit, 0);
    return hit;
}
} // namespace lv::detail
