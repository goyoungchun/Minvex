#pragma once

#include <minvex/Math.h>
#include <minvex/Asset.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lv {

inline constexpr std::size_t MaxParticleEmittersPerEffect = 16;
inline constexpr std::size_t MaxParticleEmittersPerScene = 256;
inline constexpr std::size_t MaxParticlesPerEmitter = 16384;
inline constexpr std::size_t MaxParticlesPerScene = 32768;
inline constexpr std::size_t MaxParticleBursts = 32;
inline constexpr std::size_t MaxParticleCurveKeys = 16;
inline constexpr std::size_t MaxParticleTrailsPerScene = 32768; // Additional rendered trail segments.
inline constexpr int MaxParticleTextureSize = 2048;
inline constexpr double MaxParticleStepSeconds = 86400;
inline constexpr std::size_t MaxParticleEvents = 4096;
inline constexpr std::size_t MaxParticleEventRoutes = 8;
inline constexpr std::size_t MaxParticleGraphNodes = 32;
inline constexpr std::uint8_t NoParticleNode = 255;

enum class ParticleSpace { World, Local };
enum class ParticleBlend { Alpha, Additive };
enum class ParticleShape { Box, Sphere, Disc, Cone };
enum class ParticleCollision { Off, Kill, Bounce };
enum class ParticleColliderSource { Plane, Terrain, Mesh, Scene };
enum class ParticleRendererType { Sprite, Mesh, Ribbon, Beam };
enum class ParticleSimulation { Cpu, Gpu };
enum class ParticleEventKind { Birth, Collision, Death };
enum class ParticleNodeOp { Constant, Age, Random, Add, Subtract, Multiply, Min, Max, Sine, Saturate };
using ParticleColor = std::array<float, 4>; // Linear RGBA in [0,1].

struct ParticleEventRoute {
    ParticleEventKind Event = ParticleEventKind::Death;
    std::uint8_t Target = 0; // Index in the same effect.
    std::uint16_t Count = 1;
};

struct ParticleGraphNode {
    ParticleNodeOp Op = ParticleNodeOp::Constant;
    std::uint8_t A = NoParticleNode, B = NoParticleNode;
    float Value = 0;
};

struct ParticleModuleGraph {
    // Connections can refer to any node in the graph. Evaluation order is compiled.
    std::vector<ParticleGraphNode> Nodes;
    std::uint8_t Size = NoParticleNode, Opacity = NoParticleNode, Rotation = NoParticleNode, Speed = NoParticleNode;
};

struct ParticleGeometry {
    ParticleRendererType Type = ParticleRendererType::Sprite;
    ModelHandle Mesh; // Rigid model asset. Empty uses a unit cube.
    Vec3 BeamTarget{0, 1, 0}; // Endpoint offset from each particle, in emitter space.
};

class ParticleTexture final : public std::enable_shared_from_this<ParticleTexture> {
public:
    const std::filesystem::path& Path() const noexcept { return path_; }
    int Width() const noexcept { return width_; }
    int Height() const noexcept { return height_; }
    const std::vector<std::uint8_t>& Pixels() const noexcept { return pixels_; }

private:
    friend std::shared_ptr<const ParticleTexture> LoadParticleTexture(const std::filesystem::path&);
    ParticleTexture() = default;
    ParticleTexture(const ParticleTexture&) = delete;
    ParticleTexture& operator=(const ParticleTexture&) = delete;
    std::filesystem::path path_;
    int width_ = 0, height_ = 0;
    std::vector<std::uint8_t> pixels_;
};
using ParticleTextureHandle = std::shared_ptr<const ParticleTexture>;
ParticleTextureHandle LoadParticleTexture(const std::filesystem::path& path); // PNG, straight alpha and sRGB RGB.

struct ParticleFlipbook {
    ParticleTextureHandle Texture;
    std::uint16_t Columns = 1, Rows = 1, Cycles = 1;
};

struct ParticlePlaneCollision {
    ParticleCollision Mode = ParticleCollision::Off;
    float Height = 0; // Y in simulation space.
    float Restitution = 0.5f;
    ParticleColliderSource Source = ParticleColliderSource::Plane;
};

struct ParticleTrail {
    float Seconds = 0; // Zero disables trails.
    std::uint16_t Segments = 4;
    float Width = 0.6f; // Width relative to the head sprite.
};

template<class T> struct ParticleRange { T Min{}, Max{}; };
template<class T> struct ParticleKey { float Time = 0; T Value{}; }; // Normalized lifetime.

struct ParticleBurst {
    double TimeSeconds = 0;
    std::uint32_t Count = 1;
};

struct ParticleSpawnModule {
    float Rate = 10; // Particles/second. The first rate particle is born at 1/Rate.
    double Duration = 0; // Zero for continuous emission; otherwise seconds per cycle.
    std::vector<ParticleBurst> Bursts; // Times relative to each cycle's start.
    double Delay = 0; // Seconds before the first cycle.
    std::uint32_t Cycles = 1; // 0 repeats indefinitely when Duration is positive.
};

struct ParticleInitialModule {
    ParticleRange<Vec3> Position{}, Velocity{}; // Independently sampled box axes.
    ParticleRange<float> Lifetime{1, 1}, Size{0.2f, 0.2f}; // Seconds, full width in world units.
    ParticleRange<float> Rotation{}, AngularVelocity{}; // Radians, radians/second.
};

struct ParticleEmitter {
    std::size_t Capacity = 256; // Retains the most recent birth requests at saturation.
    ParticleSpace Space = ParticleSpace::World;
    ParticleBlend Blend = ParticleBlend::Alpha;
    ParticleSpawnModule Spawn;
    ParticleInitialModule Initial;
    Vec3 Acceleration{}; // In the simulation space.
    std::vector<ParticleKey<ParticleColor>> ColorOverLife; // Empty gives opaque white.
    std::vector<ParticleKey<float>> SizeOverLife; // Multiplier. Empty gives 1.
    float AspectRatio = 1; // Height / width of the billboard.
    std::string Name;
    bool Enabled = true;
    ParticleShape Shape = ParticleShape::Box;
    float Radius = 0.5f;
    float ConeAngle = 0.5235988f; // Half angle in radians about local +Y.
    float RadialSpeed = 0; // Added along the sampled direction.
    float Drag = 0; // Linear damping, per second.
    ParticleFlipbook Flipbook;
    ParticlePlaneCollision Collision;
    ParticleTrail Trail;
    float MaxDrawDistance = 0; // Zero disables distance culling.
    bool EmitEvents = false;
    std::vector<ParticleEventRoute> EventRoutes;
    ParticleGeometry Geometry;
    ParticleSimulation Simulation = ParticleSimulation::Cpu;
    ParticleModuleGraph Graph;
};

// Immutable and shareable across Scene objects. Creation validates and copies/moves the modules.
class ParticleEffect final {
public:
    const std::vector<ParticleEmitter>& Emitters() const noexcept { return emitters_; }
    const std::vector<std::uint8_t>& EmitterOrder() const noexcept { return emitterOrder_; }
    const std::vector<std::uint8_t>& GraphOrder(std::size_t emitter) const noexcept { return graphOrder_[emitter]; }

private:
    friend std::shared_ptr<const ParticleEffect> MakeParticleEffect(std::vector<ParticleEmitter>);
    explicit ParticleEffect(std::vector<ParticleEmitter> emitters);
    ParticleEffect(const ParticleEffect&) = delete;
    ParticleEffect& operator=(const ParticleEffect&) = delete;
    std::vector<ParticleEmitter> emitters_;
    std::vector<std::uint8_t> emitterOrder_;
    std::vector<std::vector<std::uint8_t>> graphOrder_;
};
using ParticleEffectHandle = std::shared_ptr<const ParticleEffect>;
ParticleEffectHandle MakeParticleEffect(std::vector<ParticleEmitter> emitters);

struct ParticleEffectBinding {
    ParticleEffectHandle Effect;
    std::uint32_t Seed = 0;
    explicit operator bool() const noexcept { return bool(Effect); }
};

struct ParticleSprite {
    Vec3 Position{}; // World-space center.
    float Size = 0, Rotation = 0, AspectRatio = 1;
    ParticleColor Color{};
    ParticleBlend Blend = ParticleBlend::Alpha;
    // Borrowed from the owning Scene until its particle definitions change.
    const ParticleTexture* Texture = nullptr;
    std::uint16_t Columns = 1, Rows = 1, Frame = 0;
    std::optional<Vec3> TrailEnd{}; // If present, Position and TrailEnd are the segment endpoints.
    float MaxDrawDistance = 0;
    ParticleRendererType Renderer = ParticleRendererType::Sprite;
    const ModelHandle* Mesh = nullptr; // Borrowed, like Texture.
};

// Used by compute-capable renderers. GPU emitters still age and spawn on the owner thread.
struct ParticleGpuSprite {
    Vec3 Origin{}, Velocity{}, Acceleration{}; // World space.
    float Age = 0, Drag = 0;
    ParticleSprite Appearance;
};

} // namespace lv
