#include <minvex/Scene.h>
#include "ParticleColliders.h"
#include "ParticleGraph.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <tuple>
#include <stdexcept>
#include <utility>
#include <unordered_set>

namespace lv {
namespace {
constexpr float MaxRate = 100000, MaxLifetime = 3600, MaxModuleValue = 1000000;
std::atomic<std::uint64_t> nextRevision{1};

bool Finite(Vec3 v) { return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z); }

bool InRange(double v, double min, double max) { return v >= min && v <= max; }

bool ValidRange(ParticleRange<float> range, float min, float max) {
    return InRange(range.Min, min, max) && InRange(range.Max, range.Min, max);
}

bool ValidRange(ParticleRange<Vec3> range) {
    return ValidRange({range.Min.X, range.Max.X}, -MaxModuleValue, MaxModuleValue) &&
           ValidRange({range.Min.Y, range.Max.Y}, -MaxModuleValue, MaxModuleValue) &&
           ValidRange({range.Min.Z, range.Max.Z}, -MaxModuleValue, MaxModuleValue);
}

template<class T, class Predicate>
void ValidateCurve(const std::vector<ParticleKey<T>>& keys, Predicate valueIsValid) {
    if (keys.size() > MaxParticleCurveKeys) { throw std::length_error("Too many particle curve keys."); }
    float previous = -1;
    for (const auto& key : keys) {
        if (!InRange(key.Time, 0, 1) || key.Time <= previous || !valueIsValid(key.Value)) {
            throw std::invalid_argument("Particle curves require strictly increasing [0,1] times and valid values.");
        }
        previous = key.Time;
    }
}

std::uint32_t Mix(std::uint32_t seed) {
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    return seed ^ (seed >> 16);
}

float Sample(ParticleRange<float> range, std::uint32_t& random) {
    if (range.Min == range.Max) { return range.Min; }
    random += 0x9e3779b9u;
    const float fraction = static_cast<float>(Mix(random) >> 8) * (1.0f / 16777216.0f);
    return std::lerp(range.Min, range.Max, fraction);
}

float ParticleRandom(std::uint32_t seed, std::uint32_t sequence) {
    return static_cast<float>(Mix(seed ^ sequence) >> 8) * (1.0f / 16777216.0f);
}

Vec3 Sample(ParticleRange<Vec3> range, std::uint32_t& random) {
    return {Sample(ParticleRange<float>{range.Min.X, range.Max.X}, random),
            Sample(ParticleRange<float>{range.Min.Y, range.Max.Y}, random),
            Sample(ParticleRange<float>{range.Min.Z, range.Max.Z}, random)};
}

Vec3 TransformVector(const Mat4& matrix, Vec3 v) {
    const auto& m = matrix.Elements;
    return {m[0] * v.X + m[4] * v.Y + m[8] * v.Z, m[1] * v.X + m[5] * v.Y + m[9] * v.Z,
            m[2] * v.X + m[6] * v.Y + m[10] * v.Z};
}

void ApplyShape(const ParticleEmitter& emitter, std::uint32_t& random, Vec3& position, Vec3& velocity) {
    if (emitter.Shape == ParticleShape::Box && emitter.RadialSpeed == 0) { return; }
    const float angle = Sample(ParticleRange<float>{0, 2 * std::numbers::pi_v<float>}, random);
    const float x = std::cos(angle), z = std::sin(angle);
    Vec3 direction{x, 0, z};
    if (emitter.Shape == ParticleShape::Sphere || emitter.Shape == ParticleShape::Cone) {
        const float lower = emitter.Shape == ParticleShape::Sphere ? -1.0f : std::cos(emitter.ConeAngle);
        const float y = Sample(ParticleRange<float>{lower, 1}, random);
        const float radial = std::sqrt(std::max(0.0f, 1 - y * y));
        direction = {x * radial, y, z * radial};
    }
    if (emitter.Shape != ParticleShape::Box) {
        const float unit = Sample(ParticleRange<float>{0, 1}, random);
        if (emitter.Shape == ParticleShape::Sphere) {
            const float radius = emitter.Radius * std::cbrt(unit);
            position.X += direction.X * radius;
            position.Y += direction.Y * radius;
            position.Z += direction.Z * radius;
        } else {
            const float radius = emitter.Radius * std::sqrt(unit);
            position.X += x * radius;
            position.Z += z * radius;
        }
    }
    velocity.X += direction.X * emitter.RadialSpeed;
    velocity.Y += direction.Y * emitter.RadialSpeed;
    velocity.Z += direction.Z * emitter.RadialSpeed;
}

std::pair<float, float> MotionWeights(float age, float drag) {
    if (!drag) { return {age, 0.5f * age * age}; }
    const double t = age, x = drag * t;
    const double velocity = -std::expm1(-x) / drag;
    // The difference t - velocity loses precision for nearly zero drag.
    const double acceleration = x < 0.01 ? t * t * (0.5 - x / 6 + x * x / 24) : (t - velocity) / drag;
    return {static_cast<float>(velocity), static_cast<float>(acceleration)};
}

float Interpolate(float a, float b, float t) { return std::lerp(a, b, t); }

ParticleColor Interpolate(const ParticleColor& a, const ParticleColor& b, float t) {
    return {std::lerp(a[0], b[0], t), std::lerp(a[1], b[1], t),
            std::lerp(a[2], b[2], t), std::lerp(a[3], b[3], t)};
}

template<class T>
T Evaluate(const std::vector<ParticleKey<T>>& keys, float time, const T& fallback) {
    if (keys.empty()) { return fallback; }
    const auto next = std::upper_bound(keys.begin(), keys.end(), time,
        [](float t, const auto& key) { return t < key.Time; });
    if (next == keys.begin()) { return next->Value; }
    const auto& previous = *(next - 1);
    if (next == keys.end()) { return previous.Value; }
    return Interpolate(previous.Value, next->Value, (time - previous.Time) / (next->Time - previous.Time));
}
} // namespace

ParticleEffect::ParticleEffect(std::vector<ParticleEmitter> emitters) : emitters_(std::move(emitters)) {
    if (emitters_.empty()) { throw std::invalid_argument("A particle effect requires at least one emitter."); }
    if (emitters_.size() > MaxParticleEmittersPerEffect) { throw std::length_error("Too many particle emitters."); }
    std::size_t capacity = 0;
    for (auto& emitter : emitters_) {
        if (emitter.Capacity == 0) { throw std::invalid_argument("Particle capacity must be positive."); }
        if (emitter.Capacity > MaxParticlesPerEmitter || (capacity += emitter.Capacity) > MaxParticlesPerScene) {
            throw std::length_error("Particle effect exceeds the capacity limit.");
        }
        auto& spawn = emitter.Spawn;
        const auto& initial = emitter.Initial;
        const auto& flipbook = emitter.Flipbook;
        if (emitter.Name.size() > 128 || emitter.Name.find('\0') != std::string::npos ||
            (emitter.Space != ParticleSpace::World && emitter.Space != ParticleSpace::Local) ||
            (emitter.Blend != ParticleBlend::Alpha && emitter.Blend != ParticleBlend::Additive) ||
            !InRange(spawn.Rate, 0, MaxRate) || !InRange(spawn.Duration, 0, MaxParticleStepSeconds) ||
            !InRange(spawn.Delay, 0, MaxParticleStepSeconds) ||
            (spawn.Cycles != 1 && spawn.Duration < 1.0 / 240.0) ||
            (emitter.Shape != ParticleShape::Box && emitter.Shape != ParticleShape::Sphere &&
             emitter.Shape != ParticleShape::Disc && emitter.Shape != ParticleShape::Cone) ||
            !InRange(emitter.Radius, 0, MaxModuleValue) ||
            !InRange(emitter.ConeAngle, 0, std::numbers::pi_v<float> / 2) ||
            !InRange(emitter.RadialSpeed, -MaxModuleValue, MaxModuleValue) ||
            !InRange(emitter.Drag, 0, MaxModuleValue) ||
            !ValidRange(initial.Position) || !ValidRange(initial.Velocity) ||
            initial.Lifetime.Min <= 0 || !ValidRange(initial.Lifetime, 0, MaxLifetime) ||
            !ValidRange(initial.Size, 0, MaxModuleValue) ||
            !ValidRange(initial.Rotation, -MaxModuleValue, MaxModuleValue) ||
            !ValidRange(initial.AngularVelocity, -MaxModuleValue, MaxModuleValue) ||
            !InRange(emitter.AspectRatio, 0.01f, 100.0f) ||
            !ValidRange({emitter.Acceleration, emitter.Acceleration})) {
            throw std::invalid_argument("Invalid particle emitter parameters.");
        }
        if (flipbook.Texture && (flipbook.Columns == 0 || flipbook.Rows == 0 || flipbook.Cycles == 0 ||
            flipbook.Columns > 64 || flipbook.Rows > 64 || flipbook.Cycles > 64 ||
            flipbook.Texture->Width() % flipbook.Columns || flipbook.Texture->Height() % flipbook.Rows)) {
            throw std::invalid_argument("A flipbook requires an evenly divisible grid and 1 to 64 rows, columns and cycles.");
        }
        const auto& collision = emitter.Collision;
        const auto& trail = emitter.Trail;
        if ((collision.Mode != ParticleCollision::Off && collision.Mode != ParticleCollision::Kill &&
             collision.Mode != ParticleCollision::Bounce) || !InRange(collision.Height, -MaxModuleValue, MaxModuleValue) ||
            !InRange(collision.Restitution, 0, 1) || !InRange(trail.Seconds, 0, 10) ||
            !InRange(trail.Width, 0, 100) || trail.Segments < 1 || trail.Segments > 8 ||
            !InRange(emitter.MaxDrawDistance, 0, MaxModuleValue)) {
            throw std::invalid_argument("Invalid particle collision, trail or draw distance.");
        }
        if (static_cast<unsigned>(collision.Source) > static_cast<unsigned>(ParticleColliderSource::Scene) ||
            static_cast<unsigned>(emitter.Geometry.Type) > static_cast<unsigned>(ParticleRendererType::Beam) ||
            static_cast<unsigned>(emitter.Simulation) > static_cast<unsigned>(ParticleSimulation::Gpu) ||
            !ValidRange({emitter.Geometry.BeamTarget, emitter.Geometry.BeamTarget})) {
            throw std::invalid_argument("Invalid particle geometry, collider source or simulation backend.");
        }
        if (emitter.EventRoutes.size() > MaxParticleEventRoutes) { throw std::length_error("Too many particle event routes."); }
        for (const auto& route : emitter.EventRoutes) {
            if (static_cast<unsigned>(route.Event) > static_cast<unsigned>(ParticleEventKind::Death) ||
                route.Target >= emitters_.size() || !route.Count || route.Count > MaxParticlesPerEmitter) {
                throw std::invalid_argument("Invalid particle event route.");
            }
        }
        graphOrder_.push_back(detail::CompileGraph(emitter.Graph));
        if (const auto& model = emitter.Geometry.Mesh) {
            std::size_t triangles = 0;
            for (const auto& part : model->Parts) {
                if (part.SkinIndex) { throw std::invalid_argument("Particle meshes require rigid parts."); }
                (void)part.Transform.NormalMatrix();
                for (const auto& primitive : model->Meshes.at(part.MeshIndex).Primitives) {
                    ValidateMesh(primitive.Geometry);
                    triangles += primitive.Geometry.Indices.size() / 3;
                    if (triangles > 4096) { throw std::length_error("A particle mesh supports up to 4096 triangles."); }
                }
            }
            if (!triangles) { throw std::invalid_argument("Particle mesh has no rigid triangles."); }
        }
        if (spawn.Bursts.size() > MaxParticleBursts) { throw std::length_error("Too many particle bursts."); }
        for (const auto& burst : spawn.Bursts) {
            const double end = spawn.Duration > 0 ? spawn.Duration : MaxParticleStepSeconds;
            if (!InRange(burst.TimeSeconds, 0, end) || burst.Count == 0) {
                throw std::invalid_argument("Particle bursts require a positive count and a time in the emission window.");
            }
        }
        std::stable_sort(spawn.Bursts.begin(), spawn.Bursts.end(), [](const auto& a, const auto& b) {
            return a.TimeSeconds < b.TimeSeconds;
        });
        ValidateCurve(emitter.ColorOverLife, [](const ParticleColor& color) {
            return std::all_of(color.begin(), color.end(), [](float v) { return InRange(v, 0, 1); });
        });
        ValidateCurve(emitter.SizeOverLife, [](float size) { return InRange(size, 0, MaxModuleValue); });
    }
    std::array<unsigned, MaxParticleEmittersPerEffect> incoming{};
    for (const auto& emitter : emitters_) { for (const auto& route : emitter.EventRoutes) { ++incoming[route.Target]; } }
    for (std::size_t i = 0; i < emitters_.size(); ++i) {
        const auto ready = std::find(incoming.begin(), incoming.begin() + emitters_.size(), 0u);
        if (ready == incoming.begin() + emitters_.size()) { throw std::invalid_argument("Particle event routes contain a cycle."); }
        const auto index = static_cast<std::uint8_t>(ready - incoming.begin());
        emitterOrder_.push_back(index);
        *ready = std::numeric_limits<unsigned>::max();
        for (const auto& route : emitters_[index].EventRoutes) { --incoming[route.Target]; }
    }
}

ParticleEffectHandle MakeParticleEffect(std::vector<ParticleEmitter> emitters) {
    return ParticleEffectHandle(new ParticleEffect(std::move(emitters)));
}

namespace detail {
struct Particle {
    double Born = 0;
    Vec3 Position{}, Velocity{};
    float Lifetime = 0, Death = 0, Size = 0, Rotation = 0, Spin = 0;
    std::uint32_t Sequence = 0;
    float MotionAge = 0, ContactAge = std::numeric_limits<float>::infinity();
    bool Collided = false;
};

namespace {
double TimeToPlane(double height, double velocity, double acceleration, double drag, float horizon) {
    constexpr double never = std::numeric_limits<double>::infinity();
    if (height < 0 || (height == 0 && (velocity < 0 || (velocity == 0 && acceleration < 0)))) { return 0; }
    if (velocity >= 0 && acceleration >= 0) { return never; }
    if (drag == 0) {
        double hit = never;
        if (!acceleration) { hit = -height / velocity; }
        else {
            const auto discriminant = velocity * velocity - 2 * acceleration * height;
            if (discriminant < 0) { return never; }
            const auto root = std::sqrt(discriminant);
            const auto denom = -velocity + root;
            hit = denom > 0 ? 2 * height / denom : (-velocity - root) / acceleration;
        }
        return hit >= 0 && hit <= horizon ? hit : never;
    }
    double start = 0, end = horizon;
    if (velocity > 0 && acceleration < 0) {
        start = std::min(end, std::log1p(velocity * drag / -acceleration) / drag);
        if (start == end) { return never; }
    } else if (velocity < 0 && acceleration > 0) {
        end = std::min(end, std::log1p(-velocity * drag / acceleration) / drag);
    }
    const auto above = [&](double time) {
        const auto [v, a] = MotionWeights(static_cast<float>(time), static_cast<float>(drag));
        return height + v * velocity + a * acceleration > 0;
    };
    if (above(end)) { return never; }
    for (int i = 0; i < 24; ++i) {
        const auto mid = (start + end) * 0.5;
        if (above(mid)) { start = mid; }
        else { end = mid; }
    }
    return end;
}

float BounceHeight(const ParticleEmitter& emitter, const Particle& particle, float age, float originalY) {
    const auto plane = emitter.Collision.Height;
    auto height = particle.Position.Y;
    auto velocity = particle.Velocity.Y;
    const auto acceleration = emitter.Acceleration.Y;
    for (int contact = 0; contact < 8; ++contact) {
        const auto hit = TimeToPlane(static_cast<double>(height) - plane, velocity, acceleration, emitter.Drag, age);
        if (!std::isfinite(hit)) {
            if (contact == 0) { return originalY; }
            const auto [v, a] = MotionWeights(age, emitter.Drag);
            return std::max(plane, height + velocity * v + acceleration * a);
        }
        const auto elapsed = static_cast<float>(hit);
        const auto velocityWeight = MotionWeights(elapsed, emitter.Drag).first;
        const auto impact = velocity * std::exp(-emitter.Drag * elapsed) + acceleration * velocityWeight;
        velocity = std::max(0.0f, -impact * emitter.Collision.Restitution);
        age = std::max(0.0f, age - elapsed);
        height = plane;
        if ((velocity <= 0.001f && acceleration <= 0) || age == 0) { return plane; }
    }
    return plane;
}

Vec3 PositionAt(const ParticleEmitter& emitter, const Particle& p, float age) {
    const auto a = emitter.Acceleration;
    const auto [velocity, acceleration] = MotionWeights(std::max(0.0f, age - p.MotionAge), emitter.Drag);
    Vec3 position{p.Position.X + p.Velocity.X * velocity + a.X * acceleration,
                  p.Position.Y + p.Velocity.Y * velocity + a.Y * acceleration,
                  p.Position.Z + p.Velocity.Z * velocity + a.Z * acceleration};
    if (emitter.Collision.Mode == ParticleCollision::Bounce && emitter.Collision.Source == ParticleColliderSource::Plane &&
        Finite(p.Position) && Finite(p.Velocity)) {
        position.Y = BounceHeight(emitter, p, age, position.Y);
    }
    return position;
}
} // namespace

struct EmitterState;
struct ChildBirth { double Age; Vec3 Position; std::uint16_t Count; };
struct EmitterContext {
    const ParticleEffect& Effect;
    std::vector<EmitterState>& States;
    const ParticleColliders* Colliders;
    ParticleEventBatch* Published;
    ObjectId Object;
    std::uint8_t Index;
    double EndTime, Delta;
    const Mat4& World;
    const Mat4& Inverse;
    std::uint32_t Seed;

    bool Observed() const;
    void Notify(ParticleEventKind kind, const Particle& particle, double age, Vec3 position) const;
    void Collide(Particle& particle, double age, double previousAge) const;
};

struct EmitterState {
    std::vector<Particle> Live;
    std::vector<ChildBirth> Routed;
    double Time = 0, SpawnTime = 0, DelayElapsed = 0, RatePhase = 0;
    bool Started = false;
    std::uint32_t Sequence = 0;

    void Reset() noexcept {
        Live.clear();
        Routed.clear();
        Time = SpawnTime = DelayElapsed = RatePhase = 0;
        Started = false;
        Sequence = 0;
    }

    double ActiveSpan(const ParticleSpawnModule& spawn, double available) const {
        if (spawn.Duration == 0 || spawn.Cycles == 0) { return available; }
        return std::min(available, std::max(0.0, spawn.Duration * spawn.Cycles - SpawnTime));
    }

    static std::uint64_t BurstsThrough(const ParticleSpawnModule& spawn, double time, const ParticleBurst& burst) {
        if (time < burst.TimeSeconds) { return 0; }
        if (spawn.Cycles == 1) { return 1; }
        const double count = std::floor((time - burst.TimeSeconds) / spawn.Duration) + 1;
        return static_cast<std::uint64_t>(spawn.Cycles ? std::min<double>(spawn.Cycles, count) : count);
    }

    bool CanSpawn(const ParticleSpawnModule& spawn, double delta) const {
        const double wait = std::max(0.0, spawn.Delay - DelayElapsed);
        if (wait > delta) { return false; }
        const auto active = ActiveSpan(spawn, delta - wait);
        if (std::fma(active, spawn.Rate, RatePhase) >= 1) { return true; }
        for (const auto& burst : spawn.Bursts) {
            if (BurstsThrough(spawn, SpawnTime + active, burst) >
                (Started ? BurstsThrough(spawn, SpawnTime, burst) : 0)) { return true; }
        }
        return false;
    }

    void Advance(const EmitterContext& context, bool emitting);
};

bool EmitterContext::Observed() const {
    const auto& emitter = Effect.Emitters()[Index];
    return emitter.EmitEvents || !emitter.EventRoutes.empty();
}

void EmitterContext::Notify(ParticleEventKind kind, const Particle& particle, double age, Vec3 position) const {
    const auto& emitter = Effect.Emitters()[Index];
    if (emitter.Space == ParticleSpace::Local) { position = World.TransformPoint(position); }
    if (!Finite(position)) { return; }
    age = std::clamp(age, 0.0, Delta);
    if (Published && emitter.EmitEvents) {
        if (Published->Events.size() < MaxParticleEvents) {
            Published->Events.push_back({Object, Index, kind, particle.Sequence, EndTime - age, position});
        } else { ++Published->Dropped; }
    }
    for (const auto& route : emitter.EventRoutes) {
        if (route.Event != kind || !Effect.Emitters()[route.Target].Enabled) { continue; }
        auto& births = States[route.Target].Routed;
        if (births.size() < births.capacity()) { births.push_back({age, position, route.Count}); }
    }
}

void EmitterContext::Collide(Particle& p, double age, double previousAge) const {
    const auto& emitter = Effect.Emitters()[Index];
    if (emitter.Collision.Mode == ParticleCollision::Off || !Finite(p.Position) || !Finite(p.Velocity)) { return; }
    const auto observe = Observed();
    if (emitter.Collision.Source == ParticleColliderSource::Plane) {
        if (observe && !p.Collided && age >= p.ContactAge) {
            auto position = PositionAt(emitter, p, p.ContactAge);
            position.Y = emitter.Collision.Height;
            Notify(ParticleEventKind::Collision, p, age - p.ContactAge, position);
            p.Collided = true;
        }
        return;
    }
    if (!Colliders) { return; }
    const auto endAge = static_cast<float>(std::min<double>(p.Death, age));
    auto startAge = static_cast<float>(std::max<double>(p.MotionAge, previousAge));
    if (endAge <= startAge) { return; }
    const auto segments = static_cast<int>(std::clamp(std::ceil((endAge - startAge) * 60.0), 1.0, 16.0));
    const auto first = startAge;
    const bool local = emitter.Space == ParticleSpace::Local;
    const auto point = [&](float t) {
        const auto position = PositionAt(emitter, p, t);
        return local ? World.TransformPoint(position) : position;
    };
    for (int i = 1; i <= segments; ++i) {
        const auto next = std::lerp(first, endAge, static_cast<float>(i) / static_cast<float>(segments));
        const auto a = point(startAge), b = point(next);
        if (const auto hit = Colliders->Trace(a, b, emitter.Collision.Source)) {
            const auto at = std::lerp(startAge, next, hit->Fraction);
            const auto worldPoint = ParticleAdd(a, ParticleScale(ParticleSubtract(b,a),hit->Fraction));
            const auto simulationPoint = local ? Inverse.TransformPoint(worldPoint) : worldPoint;
            if (observe && !p.Collided) { Notify(ParticleEventKind::Collision, p, age - at, simulationPoint); }
            p.Collided = true;
            if (emitter.Collision.Mode == ParticleCollision::Kill) { p.Death = at; p.Position = simulationPoint; return; }
            const auto travel = at - p.MotionAge;
            const auto damping = std::exp(-emitter.Drag * travel);
            auto velocity = ParticleAdd(ParticleScale(p.Velocity, damping),
                ParticleScale(emitter.Acceleration, MotionWeights(travel, emitter.Drag).first));
            if (local) { velocity = TransformVector(World, velocity); }
            velocity = ParticleSubtract(velocity, ParticleScale(hit->Normal,
                std::min(0.0f, ParticleDot(velocity, hit->Normal)) * (1 + emitter.Collision.Restitution)));
            if (local) { velocity = TransformVector(Inverse, velocity); }
            const auto offset = ParticleAdd(worldPoint, ParticleScale(hit->Normal, 0.0001f));
            p.Position = local ? Inverse.TransformPoint(offset) : offset;
            p.Velocity = velocity;
            p.MotionAge = at;
        }
        startAge = next;
    }
}

void EmitterState::Advance(const EmitterContext& context, bool emitting) {
    const auto& emitter = context.Effect.Emitters()[context.Index];
    const auto delta = context.Delta;
    const auto& world = context.World;
    const auto seed = context.Seed;
    const bool observe = context.Observed();
    Time = Live.empty() ? 0 : Time + delta;
    const auto& spawn = emitter.Spawn;
    struct Cursor {
        double Age;
        std::uint64_t Cycles;
        std::uint32_t Left;
        std::size_t Index;
    };
    std::array<Cursor, MaxParticleBursts> bursts;
    auto endBurst = bursts.begin();
    const auto newest = [](const Cursor& a, const Cursor& b) {
        return a.Age == b.Age ? a.Index < b.Index : a.Age > b.Age;
    };
    std::uint64_t rates = 0, requests = 0;
    double tail = 0;
    if (!emitting || !emitter.Enabled) { Routed.clear(); }
    if (emitting && emitter.Enabled) {
        const double wait = std::max(0.0, spawn.Delay - DelayElapsed);
        DelayElapsed += std::min(wait, delta);
        if (wait <= delta) {
            const double active = ActiveSpan(spawn, delta - wait);
            const double end = SpawnTime + active;
            tail = delta - wait - active;
            const double accrued = std::fma(active, spawn.Rate, RatePhase);
            rates = static_cast<std::uint64_t>(accrued);
            RatePhase = accrued - static_cast<double>(rates);
            requests = rates;
            for (std::size_t i = 0; i < spawn.Bursts.size(); ++i) {
                const auto& burst = spawn.Bursts[i];
                const auto through = BurstsThrough(spawn, end, burst);
                const auto previous = Started ? BurstsThrough(spawn, SpawnTime, burst) : 0;
                if (through == previous) { continue; }
                const auto age = std::max(0.0, end - (burst.TimeSeconds + static_cast<double>(through - 1) * spawn.Duration));
                *endBurst++ = {tail + age, through - previous, burst.Count, i};
                requests += (through - previous) * burst.Count;
            }
            std::make_heap(bursts.begin(), endBurst, newest);
            Started = true;
            SpawnTime = spawn.Duration == 0 ?
                std::min(end, spawn.Bursts.empty() ? 0 : spawn.Bursts.back().TimeSeconds) : end;
        }
    }
    std::sort(Routed.begin(), Routed.end(), [](const auto& a, const auto& b) { return a.Age > b.Age; });
    for (const auto& child : Routed) { requests += child.Count; }
    if (observe || (emitter.Collision.Mode != ParticleCollision::Off && emitter.Collision.Source != ParticleColliderSource::Plane)) {
        for (auto& p : Live) {
            const auto age = Time - p.Born;
            context.Collide(p, age, age - delta);
            if (observe && age >= p.Death) {
                const bool meshKill = p.Collided && emitter.Collision.Mode == ParticleCollision::Kill &&
                    emitter.Collision.Source != ParticleColliderSource::Plane;
                context.Notify(ParticleEventKind::Death, p, age - p.Death, meshKill ? p.Position : PositionAt(emitter, p, p.Death));
            }
        }
    }
    Sequence += static_cast<std::uint32_t>(requests);
    if (requests >= emitter.Capacity) { Live.clear(); }
    else {
        // Count requests, including particles that already expired. Splitting a
        // step must not let a particle survive the newer request that replaced it.
        std::erase_if(Live, [&](const Particle& p) {
            return Sequence - p.Sequence >= emitter.Capacity || Time - p.Born >= p.Death;
        });
    }
    if (!requests) { return; }

    const auto& initial = emitter.Initial;
    const auto firstNew = Live.size();
    constexpr auto infinity = std::numeric_limits<double>::infinity();
    double rateAge = rates ? tail + RatePhase / spawn.Rate : infinity;
    const auto rateCount = rates;
    const auto limit = std::min<std::uint64_t>(requests, emitter.Capacity);
    // Walk the newest requests first, so an oversized burst does bounded work.
    // On an equal timestamp the burst is newer than the rate request.
    for (std::uint32_t i = 0; i < limit; ++i) {
        const double burstAge = bursts.begin() == endBurst ? infinity : bursts.front().Age;
        const double childAge = Routed.empty() ? infinity : Routed.back().Age;
        const bool useChild = childAge <= std::min(burstAge, rateAge);
        const bool useBurst = !useChild && burstAge <= rateAge;
        const auto age = std::min({burstAge, rateAge, childAge});
        if (!observe && age >= initial.Lifetime.Max) { break; }
        const auto sequence = Sequence - i;
        auto random = Mix(seed ^ sequence);
        const auto lifetime = Sample(initial.Lifetime, random);
        if (observe || age < lifetime) {
            auto position = Sample(initial.Position, random);
            auto velocity = Sample(initial.Velocity, random);
            ApplyShape(emitter, random, position, velocity);
            if (emitter.Graph.Speed != NoParticleNode) {
                const auto graph = EvaluateGraph(emitter.Graph, context.Effect.GraphOrder(context.Index), 0, ParticleRandom(seed, sequence));
                velocity = ParticleScale(velocity, graph.Speed);
            }
            if (emitter.Space == ParticleSpace::World) {
                position = useChild ? ParticleAdd(Routed.back().Position, TransformVector(world, position)) : world.TransformPoint(position);
                velocity = TransformVector(world, velocity);
            } else if (useChild) { position = ParticleAdd(context.Inverse.TransformPoint(Routed.back().Position), position); }
            Particle p{Time - age, position, velocity, lifetime, lifetime, Sample(initial.Size, random),
                       Sample(initial.Rotation, random), Sample(initial.AngularVelocity, random), sequence};
            if (emitter.Collision.Source == ParticleColliderSource::Plane && emitter.Collision.Mode != ParticleCollision::Off &&
                (observe || emitter.Collision.Mode == ParticleCollision::Kill) && Finite(position) && Finite(velocity)) {
                const auto hit = TimeToPlane(static_cast<double>(position.Y) - emitter.Collision.Height,
                    velocity.Y, emitter.Acceleration.Y, emitter.Drag, lifetime);
                p.ContactAge = static_cast<float>(hit);
                if (emitter.Collision.Mode == ParticleCollision::Kill && hit < p.Death) { p.Death = p.ContactAge; }
            }
            if (observe) { context.Notify(ParticleEventKind::Birth, p, age, position); }
            context.Collide(p, age, 0);
            if (age < p.Death) { Live.push_back(p); }
            else if (observe) {
                const bool meshKill = p.Collided && emitter.Collision.Mode == ParticleCollision::Kill &&
                    emitter.Collision.Source != ParticleColliderSource::Plane;
                context.Notify(ParticleEventKind::Death, p, age - p.Death, meshKill ? p.Position : PositionAt(emitter, p, p.Death));
            }
        }
        if (useChild) {
            if (--Routed.back().Count == 0) { Routed.pop_back(); }
        } else if (useBurst) {
            if (--bursts.front().Left == 0) {
                std::pop_heap(bursts.begin(), endBurst, newest);
                auto& cursor = *--endBurst;
                if (--cursor.Cycles) {
                    cursor.Age += spawn.Duration;
                    cursor.Left = spawn.Bursts[cursor.Index].Count;
                    std::push_heap(bursts.begin(), ++endBurst, newest);
                }
            }
        } else {
            --rates;
            rateAge = rates ? tail + (RatePhase + static_cast<double>(rateCount - rates)) / spawn.Rate : infinity;
        }
    }
    Routed.clear();
    std::reverse(Live.begin() + static_cast<std::ptrdiff_t>(firstNew), Live.end());
}

struct ParticleInstance {
    ParticleEffectHandle Effect;
    std::vector<EmitterState> States;
    std::size_t Capacity = 0, Live = 0;
    std::uint64_t Revision = 0;
    std::uint32_t Seed;
    Mat4 SpawnWorld;
    Mat4 SpawnInverse;
    unsigned ColliderMask = 0;
    bool PublishesEvents = false, HasRoutes = false, NeedsInverse = false, NeedsEventTransform = false;
    double PlaybackTime = 0;
    bool Emitting = true, Prepared = false;

    ParticleInstance(ParticleEffectHandle effect, std::uint32_t seed)
        : Effect(std::move(effect)), States(Effect->Emitters().size()), Seed(seed) {
        bool hasLocal = false;
        for (const auto& emitter : Effect->Emitters()) {
            Capacity += emitter.Capacity;
            if (!emitter.Enabled) { continue; }
            const auto mask = emitter.Collision.Mode == ParticleCollision::Off ? 0 : ParticleColliderMask(emitter.Collision.Source);
            ColliderMask |= mask;
            PublishesEvents |= emitter.EmitEvents;
            HasRoutes |= !emitter.EventRoutes.empty();
            const auto local = emitter.Space == ParticleSpace::Local;
            hasLocal |= local;
            NeedsEventTransform |= local && (emitter.EmitEvents || mask != 0);
            NeedsInverse |= local && mask != 0;
        }
        NeedsInverse |= HasRoutes && hasLocal;
    }

    void Reserve() {
        if (Prepared) { return; }
        std::array<bool, MaxParticleEmittersPerEffect> routed{};
        for (const auto& emitter : Effect->Emitters()) {
            for (const auto& route : emitter.EventRoutes) { routed[route.Target] = true; }
        }
        for (std::size_t i = 0; i < States.size(); ++i) {
            const auto& emitter = Effect->Emitters()[i];
            if (!emitter.Enabled) { continue; }
            const auto& spawn = emitter.Spawn;
            const double window = spawn.Duration > 0 && spawn.Cycles ?
                std::min<double>(spawn.Duration * spawn.Cycles, emitter.Initial.Lifetime.Max) : emitter.Initial.Lifetime.Max;
            std::uint64_t estimate = spawn.Rate > 0 ? static_cast<std::uint64_t>(std::ceil(window * spawn.Rate)) + 1 : 0;
            const auto repeats = spawn.Cycles == 1 ? 1 : static_cast<std::uint64_t>(std::ceil(window / spawn.Duration)) + 1;
            for (const auto& burst : spawn.Bursts) { estimate += repeats * burst.Count; }
            States[i].Live.reserve(routed[i] ? emitter.Capacity : static_cast<std::size_t>(std::min<std::uint64_t>(emitter.Capacity, estimate)));
            if (routed[i]) { States[i].Routed.reserve(std::min<std::size_t>(emitter.Capacity, MaxParticleEvents)); }
        }
        Prepared = true;
    }

    void PrepareTransform(const Scene& scene, ObjectId id, double delta, bool replay) {
        const auto& emitters = Effect->Emitters();
        bool required = HasRoutes || NeedsEventTransform;
        for (std::size_t i = 0; !required && i < emitters.size(); ++i) {
            const auto& e = emitters[i];
            required = e.Enabled && e.Space == ParticleSpace::World && (replay || (Emitting && States[i].CanSpawn(e.Spawn, delta)));
        }
        if (!required) { return; }
        SpawnWorld = scene.GetWorldTransform(id);
        if (NeedsInverse) { SpawnInverse = SpawnWorld.InverseAffine(); }
    }

    void Advance(ObjectId id, double delta, const ParticleColliders* colliders, ParticleEventBatch* events) {
        PlaybackTime += delta;
        Live = 0;
        for (const auto i : Effect->EmitterOrder()) {
            auto& state = States[i];
            state.Advance({*Effect, States, colliders, events, id, i, PlaybackTime, delta, SpawnWorld, SpawnInverse,
                           Mix(Seed + i)}, Emitting);
            Live += state.Live.size();
        }
    }
};

struct SceneParticles {
    std::map<ObjectId, ParticleInstance> Instances; // Stable order for equal-depth blending.
    std::size_t Capacity = 0, Emitters = 0, Live = 0;
    ParticleColliders Colliders;
    ParticleEventBatch Events;
};
} // namespace detail

Scene::Scene() = default;
Scene::~Scene() = default;
Scene::Scene(Scene&&) noexcept = default;
Scene& Scene::operator=(Scene&&) noexcept = default;

void Scene::SetParticleEffect(ObjectId id, ParticleEffectHandle effect, std::uint32_t seed) {
    (void)GetObject(id);
    if (!effect) { EraseParticleEffect(id); return; }
    auto created = particles_ ? nullptr : std::make_unique<detail::SceneParticles>();
    auto& system = particles_ ? *particles_ : *created;
    const auto old = system.Instances.find(id);
    const auto replaced = old == system.Instances.end() ? nullptr : &old->second;
    auto capacity = system.Capacity - (replaced ? replaced->Capacity : 0);
    const auto emitters = system.Emitters - (replaced ? replaced->States.size() : 0) + effect->Emitters().size();
    for (const auto& emitter : effect->Emitters()) { capacity += emitter.Capacity; }
    if (capacity > MaxParticlesPerScene || emitters > MaxParticleEmittersPerScene) {
        throw std::length_error("Scene particle effect exceeds the emitter or particle capacity limit.");
    }
    std::unordered_set<const ParticleTexture*> textures;
    std::size_t textureBytes = 0;
    const auto addTextures = [&](const ParticleEffect& definition) {
        for (const auto& emitter : definition.Emitters()) {
            const auto* texture = emitter.Flipbook.Texture.get();
            if (!texture || !textures.insert(texture).second) { continue; }
            textureBytes += texture->Pixels().size();
            if (textures.size() > 32 || textureBytes > 64 * 1024 * 1024) {
                throw std::length_error("Scene particle textures exceed 32 images or 64 MiB decoded.");
            }
        }
    };
    for (const auto& [object, instance] : system.Instances) {
        if (object != id) { addTextures(*instance.Effect); }
    }
    addTextures(*effect);
    detail::ParticleInstance instance(std::move(effect), seed);
    instance.Reserve();
    const auto removed = replaced ? replaced->Live : 0;
    system.Instances.insert_or_assign(id, std::move(instance));
    system.Capacity = capacity;
    system.Emitters = emitters;
    system.Live -= removed;
    if (created) { particles_ = std::move(created); }
}

void Scene::EraseParticleEffect(ObjectId id) noexcept {
    if (!particles_) { return; }
    auto& system = *particles_;
    const auto found = system.Instances.find(id);
    if (found == system.Instances.end()) { return; }
    system.Capacity -= found->second.Capacity;
    system.Emitters -= found->second.States.size();
    system.Live -= found->second.Live;
    system.Instances.erase(found);
    if (system.Instances.empty()) { particles_.reset(); }
}

void Scene::CopyParticleEffectsTo(Scene& snapshot) const {
    if (!particles_) { return; }
    auto definitions = std::make_unique<detail::SceneParticles>();
    definitions->Capacity = particles_->Capacity;
    definitions->Emitters = particles_->Emitters;
    for (const auto& [id, instance] : particles_->Instances) {
        definitions->Instances.try_emplace(id, instance.Effect, instance.Seed);
    }
    snapshot.particles_ = std::move(definitions);
}

void Scene::ReuseParticlePreviewsFrom(Scene& previous) noexcept {
    if (!particles_ || !previous.particles_) { return; }
    for (auto& [id, instance] : particles_->Instances) {
        const auto old = previous.particles_->Instances.find(id);
        if (old != previous.particles_->Instances.end() && instance.Effect == old->second.Effect &&
            instance.Seed == old->second.Seed) {
            std::swap(instance, old->second);
            particles_->Live += instance.Live;
        }
    }
}

ParticleEffectBinding Scene::GetParticleEffect(ObjectId id) const {
    (void)GetObject(id);
    if (particles_) {
        const auto found = particles_->Instances.find(id);
        if (found != particles_->Instances.end()) { return {found->second.Effect, found->second.Seed}; }
    }
    return {};
}

namespace {
detail::ParticleInstance& RequireInstance(detail::SceneParticles* system, ObjectId id) {
    if (system) {
        const auto found = system->Instances.find(id);
        if (found != system->Instances.end()) { return found->second; }
    }
    throw std::invalid_argument("Object has no particle effect.");
}
}

void Scene::RestartParticles(ObjectId id) {
    auto& instance = RequireInstance(particles_.get(), id);
    particles_->Live -= instance.Live;
    instance.Live = 0;
    for (auto& state : instance.States) { state.Reset(); }
    instance.Emitting = true;
    instance.PlaybackTime = 0;
}

void Scene::SeekParticles(ObjectId id, double seconds) {
    if (!InRange(seconds, 0, MaxParticleStepSeconds)) {
        throw std::invalid_argument("Particle timeline must be within [0,86400] seconds.");
    }
    auto& instance = RequireInstance(particles_.get(), id);
    instance.Reserve();
    instance.PrepareTransform(*this, id, seconds, true);
    if (instance.ColliderMask) { particles_->Colliders.Prepare(*this, instance.ColliderMask); }
    RestartParticles(id);
    instance.Advance(id, seconds, instance.ColliderMask ? &particles_->Colliders : nullptr, nullptr);
    if (instance.Live) { instance.Revision = nextRevision.fetch_add(1, std::memory_order_relaxed); }
    particles_->Live += instance.Live;
}

double Scene::ParticleTime(ObjectId id) const {
    return std::min(MaxParticleStepSeconds, RequireInstance(particles_.get(), id).PlaybackTime);
}

void Scene::SetParticleEmission(ObjectId id, bool enabled) {
    RequireInstance(particles_.get(), id).Emitting = enabled;
}

void Scene::UpdateParticles(double delta) {
    if (!InRange(delta, 0, MaxParticleStepSeconds)) {
        throw std::invalid_argument("Particle delta must be finite and in [0,86400] seconds.");
    }
    if (!particles_) { return; }
    auto& system = *particles_;
    unsigned colliders = 0;
    bool publish = false;
    for (auto& [id, instance] : system.Instances) {
        instance.Reserve();
        instance.PrepareTransform(*this, id, delta, false);
        colliders |= instance.ColliderMask;
        publish |= instance.PublishesEvents;
    }
    if (colliders) { system.Colliders.Prepare(*this, colliders); }
    else { system.Colliders = {}; }
    if (publish) { system.Events.Events.reserve(MaxParticleEvents); }
    std::size_t total = 0;
    std::uint64_t revision = 0;
    const auto firstEvent = system.Events.Events.size();
    for (auto& [id, instance] : system.Instances) {
        const auto previous = instance.Live;
        instance.Advance(id, delta, colliders ? &system.Colliders : nullptr, publish ? &system.Events : nullptr);
        if (instance.Live && (delta > 0 || instance.Live != previous)) {
            if (!revision) { revision = nextRevision.fetch_add(1, std::memory_order_relaxed); }
            instance.Revision = revision;
        }
        total += instance.Live;
    }
    std::stable_sort(system.Events.Events.begin() + static_cast<std::ptrdiff_t>(firstEvent), system.Events.Events.end(),
        [](const SceneParticleEvent& a, const SceneParticleEvent& b) {
            return std::tie(a.Object,a.Time) < std::tie(b.Object,b.Time);
        });
    system.Live = total;
}

ParticleEventBatch Scene::PollParticleEvents() {
    if (!particles_) { return {}; }
    auto result = particles_->Events;
    particles_->Events.Events.clear();
    particles_->Events.Dropped = 0;
    return result;
}

std::size_t Scene::ParticleCount(ObjectId id) const {
    if (!id) { return particles_ ? particles_->Live : 0; }
    (void)GetObject(id);
    if (particles_) {
        const auto found = particles_->Instances.find(id);
        if (found != particles_->Instances.end()) { return found->second.Live; }
    }
    return 0;
}

bool Scene::ParticleObjectVisible(ObjectId id) const {
    while (id) {
        const auto& object = GetObject(id);
        if (!object.Visible) { return false; }
        id = object.Parent;
    }
    return true;
}

void Scene::CollectParticleSprites(std::vector<ParticleSprite>& sprites, std::vector<ParticleGpuSprite>* gpu) const {
    sprites.clear();
    if (gpu) { gpu->clear(); }
    if (!particles_ || !particles_->Live) { return; }
    if (sprites.capacity() < particles_->Live) { sprites.reserve(std::bit_ceil(particles_->Live)); }
    std::size_t trailBudget = MaxParticleTrailsPerScene;
    for (const auto& [id, instance] : particles_->Instances) {
        if (!instance.Live || !ParticleObjectVisible(id)) { continue; }
        std::optional<Mat4> world;
        for (std::size_t i = 0; i < instance.States.size(); ++i) {
            const auto& state = instance.States[i];
            if (state.Live.empty()) { continue; }
            const auto& emitter = instance.Effect->Emitters()[i];
            const bool local = emitter.Space == ParticleSpace::Local;
            const auto renderer = emitter.Geometry.Type;
            if ((local || renderer == ParticleRendererType::Beam) && !world) { world = GetWorldTransform(id); }
            const bool compute = gpu && emitter.Simulation == ParticleSimulation::Gpu &&
                renderer == ParticleRendererType::Sprite && emitter.Blend == ParticleBlend::Additive &&
                emitter.Collision.Mode == ParticleCollision::Off && !emitter.Trail.Seconds && !emitter.Flipbook.Texture;
            std::optional<Vec3> ribbonPrevious;
            const auto seed = Mix(instance.Seed + static_cast<std::uint32_t>(i));
            const auto& graphOrder = instance.Effect->GraphOrder(i);
            for (const auto& p : state.Live) {
                const auto age = static_cast<float>(state.Time - p.Born);
                const auto normalized = age / p.Lifetime;
                const auto graph = graphOrder.empty() ? detail::GraphValues{} :
                    detail::EvaluateGraph(emitter.Graph, graphOrder, normalized, ParticleRandom(seed, p.Sequence));
                const auto size = p.Size * Evaluate(emitter.SizeOverLife, normalized, 1.0f) * graph.Size;
                auto color = Evaluate(emitter.ColorOverLife, normalized, ParticleColor{1, 1, 1, 1});
                color[3] *= graph.Opacity;
                if (size <= 0 || color[3] <= 0) { continue; }
                ParticleSprite sprite{{}, size, p.Rotation + p.Spin * age + graph.Rotation, emitter.AspectRatio, color, emitter.Blend};
                sprite.MaxDrawDistance = emitter.MaxDrawDistance;
                sprite.Renderer = renderer;
                if (renderer == ParticleRendererType::Mesh && emitter.Geometry.Mesh) { sprite.Mesh = &emitter.Geometry.Mesh; }
                if (compute) {
                    const auto origin = local ? world->TransformPoint(p.Position) : p.Position;
                    const auto velocity = local ? TransformVector(*world, p.Velocity) : p.Velocity;
                    const auto accel = local ? TransformVector(*world, emitter.Acceleration) : emitter.Acceleration;
                    if (Finite(origin) && Finite(velocity) && Finite(accel)) {
                        gpu->push_back({origin, velocity, accel, age, emitter.Drag, sprite});
                    }
                    continue;
                }
                auto position = detail::PositionAt(emitter, p, age);
                if (local) { position = world->TransformPoint(position); }
                if (!Finite(position)) { continue; }
                sprite.Position = position;
                const auto& flipbook = emitter.Flipbook;
                if (flipbook.Texture) {
                    const unsigned frames = flipbook.Columns * flipbook.Rows;
                    const unsigned total = frames * flipbook.Cycles;
                    const auto frame = std::min(total - 1, static_cast<unsigned>(normalized * static_cast<float>(total)));
                    sprite.Texture = flipbook.Texture.get();
                    sprite.Columns = flipbook.Columns;
                    sprite.Rows = flipbook.Rows;
                    sprite.Frame = static_cast<std::uint16_t>(frame % frames);
                }
                if (renderer == ParticleRendererType::Ribbon) {
                    if (ribbonPrevious) {
                        sprite.Position = *ribbonPrevious;
                        sprite.TrailEnd = position;
                        sprites.push_back(sprite);
                    }
                    ribbonPrevious = position;
                    continue;
                }
                if (renderer == ParticleRendererType::Beam) {
                    sprite.TrailEnd = detail::ParticleAdd(position, TransformVector(*world, emitter.Geometry.BeamTarget));
                }
                sprites.push_back(sprite);
                if (renderer != ParticleRendererType::Sprite) { continue; }
                const auto& trail = emitter.Trail;
                if (!trailBudget || !trail.Seconds || !trail.Width || age <= 0) { continue; }
                const auto count = std::min<std::size_t>(trail.Segments, trailBudget);
                const auto span = std::min(age - p.MotionAge, trail.Seconds);
                if (span <= 0) { continue; }
                auto previous = position;
                for (std::size_t segment = 0; segment < count; ++segment) {
                    const auto fraction = static_cast<float>(segment + 1) / static_cast<float>(count);
                    auto end = detail::PositionAt(emitter, p, age - fraction * span);
                    if (local) { end = world->TransformPoint(end); }
                    if (!Finite(end)) { break; }
                    auto tint = color;
                    tint[3] *= 1 - fraction * 0.9f;
                    ParticleSprite piece{previous, size * trail.Width * (1 - fraction * 0.75f), 0, 1, tint, emitter.Blend};
                    piece.TrailEnd = end;
                    piece.MaxDrawDistance = emitter.MaxDrawDistance;
                    sprites.push_back(piece);
                    previous = end;
                    --trailBudget;
                }
            }
        }
    }
}

std::uint64_t Scene::ParticleRenderSignature() const {
    if (!particles_ || !particles_->Live) { return 0; }
    std::uint64_t hash = 0;
    const auto mix = [&](std::uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };
    for (const auto& [id, instance] : particles_->Instances) {
        if (!instance.Live || !ParticleObjectVisible(id)) { continue; }
        mix(id);
        mix(instance.Revision);
        for (std::size_t i = 0; i < instance.States.size(); ++i) {
            const auto& emitter = instance.Effect->Emitters()[i];
            if (!instance.States[i].Live.empty() &&
                (emitter.Space == ParticleSpace::Local || emitter.Geometry.Type == ParticleRendererType::Beam)) {
                const auto world = GetWorldTransform(id);
                for (const auto value : world.Elements) { mix(std::bit_cast<std::uint32_t>(value)); }
                break;
            }
        }
    }
    return hash;
}
} // namespace lv
