#pragma once

#include <minvex/Math.h>

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace lv {

inline constexpr std::size_t MaxPhysicsBoxes = 256;
struct PhysicsBox {
    Bounds Collider;
    bool Static = false;
};
struct BoxSupport {
    double Gap = std::numeric_limits<double>::infinity(); // Distance to the closest top below the box.
    double ContactFraction = 0; // Union of contacting footprints divided by the box footprint.
    bool CenterOverContact = false;
};
// Negative Y is down. Only positive-area contact counts; tolerance is in metres.
BoxSupport CheckBoxSupport(std::span<const PhysicsBox> boxes, std::size_t box, double tolerance = 0.001);

struct GravityPreviewOptions { double Seconds = 1, Acceleration = 9.80665; };
struct BoxGravityResult {
    PhysicsBox Box;
    bool InContact = false;
};
// Evaluate a copy of axis-aligned boxes initially at rest. Translation is
// vertical only; require disjoint interiors for every moving box at entry.
std::vector<BoxGravityResult> PreviewBoxGravity(std::span<const PhysicsBox> boxes, GravityPreviewOptions options = {});

struct RigidBox {
    Vec3 Position{};
    Quaternion Rotation{};
    Vec3 HalfExtents{0.5f,0.5f,0.5f};
    float Mass = 1; // Kilograms, zero makes the collider static.
    float Friction = 0.55f; // Coulomb coefficient; geometric mean at a contact.
    Vec3 LinearVelocity{}; // World metres/second.
    Vec3 AngularVelocity{}; // World radians/second.
};

struct RigidBoxStepOptions {
    double Seconds = 1.0/60;
    Vec3 Gravity{0,-9.80665f,0};
};
struct RigidBoxContact {
    std::size_t First = 0, Second = 0; // Input box indices.
    Vec3 Position{};
    Vec3 Normal{}; // Unit normal from First toward Second.
    float Penetration = 0; // Nonnegative overlap in metres.
};
struct RigidBoxStep {
    std::vector<RigidBox> Boxes; // Input order, independently owned.
    std::vector<RigidBoxContact> Contacts; // Contacts solved in the last substep.
};

// Discrete, inelastic oriented-box dynamics over at most one second per call.
// Zero duration queries the current contact geometry without advancing poses.
// Pass the returned boxes into the next step to continue the simulation.
// Input boxes are never modified. Static boxes keep their pose.
RigidBoxStep StepRigidBoxes(std::span<const RigidBox> boxes, RigidBoxStepOptions options = {});

} // namespace lv
