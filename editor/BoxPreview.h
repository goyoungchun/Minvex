#pragma once

#include <minvex/EditorDocument.h>
#include <minvex/Physics.h>

namespace lv::editor {
struct BoxCollider {
    ObjectId Object = NoObject;
    bool Static = false;
    float Mass = 1, Friction = 0.55f;
    Vec3 LinearVelocity{}, AngularVelocity{};
};
enum class BoxPreviewMode { Support, Gravity, Rigid };
struct BoxPreviewOptions {
    double Seconds = 0;
    double Acceleration = 9.80665, SupportTolerance = 0.001;
    Vec3 Gravity{0,-9.80665f,0};
};
struct BoxPreview {
    struct Source {
        ObjectId Object;
        ModelHandle Model;
        Mat4 World, Destination;
    };
    BoxPreviewMode Mode = BoxPreviewMode::Support;
    std::vector<Source> Sources;
    std::vector<PhysicsBox> AxisAligned;
    std::vector<BoxSupport> Support;
    std::vector<bool> Grounded;
    std::vector<RigidBox> InitialRigid;
    RigidBoxStep Rigid;
    double Seconds = 0;

    bool Matches(const Scene& scene) const;
    bool HasChanges() const;
    void StepRigid(RigidBoxStepOptions options);
};

BoxPreview PreviewBoxes(const Scene& scene, std::span<const BoxCollider> colliders, BoxPreviewMode mode, BoxPreviewOptions options = {});
Result<void> ApplyBoxPreview(EditorDocument& document, const BoxPreview& preview);
} // namespace lv::editor
