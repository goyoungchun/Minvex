#pragma once

#include <minvex/Scene.h>

namespace lv::detail {

struct ParticleHit {
    float Fraction = 0;
    Vec3 Normal{};
};

class ParticleColliders {
public:
    void Prepare(const Scene& scene, unsigned mask);
    std::optional<ParticleHit> Trace(Vec3 start, Vec3 end, ParticleColliderSource source) const noexcept;

private:
    struct Triangle { Vec3 A, B, C; Bounds Box; unsigned Mask; };
    struct Node { Bounds Box; std::size_t First, Count, Right; unsigned Mask; };
    std::size_t Build(std::size_t first, std::size_t last);
    std::vector<Triangle> triangles_;
    std::vector<Node> nodes_;
    std::vector<ModelHandle> models_;
    std::uint64_t signature_ = 0;
};

Vec3 ParticleAdd(Vec3 a, Vec3 b) noexcept;
Vec3 ParticleSubtract(Vec3 a, Vec3 b) noexcept;
Vec3 ParticleScale(Vec3 a, float scale) noexcept;
float ParticleDot(Vec3 a, Vec3 b) noexcept;
unsigned ParticleColliderMask(ParticleColliderSource source) noexcept;
} // namespace lv::detail
