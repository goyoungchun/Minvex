#include "BoxPreview.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>

namespace lv::editor {
namespace {
constexpr float MinimumHalfExtent = 0.005f;

bool IsVisible(const Scene& scene, ObjectId id) {
    for (auto current = id; current; current = scene.GetObject(current).Parent) {
        if (!scene.GetObject(current).Visible) { return false; }
    }
    return true;
}

Bounds WorldBounds(Bounds local, const Mat4& world) {
    const auto first = world.TransformPoint(local.Min);
    Bounds bounds{first,first};
    for (unsigned i=1; i<8; ++i) {
        const auto p = world.TransformPoint({i&1 ? local.Max.X : local.Min.X,
                                             i&2 ? local.Max.Y : local.Min.Y,
                                             i&4 ? local.Max.Z : local.Min.Z});
        bounds.Min = {std::min(bounds.Min.X,p.X),std::min(bounds.Min.Y,p.Y),std::min(bounds.Min.Z,p.Z)};
        bounds.Max = {std::max(bounds.Max.X,p.X),std::max(bounds.Max.Y,p.Y),std::max(bounds.Max.Z,p.Z)};
    }
    const auto pad = [](float& min, float& max) {
        if (max-min >= 2*MinimumHalfExtent) { return; }
        const auto mid = std::midpoint(min,max);
        min = mid-MinimumHalfExtent;
        max = mid+MinimumHalfExtent;
    };
    pad(bounds.Min.X,bounds.Max.X);
    pad(bounds.Min.Y,bounds.Max.Y);
    pad(bounds.Min.Z,bounds.Max.Z);
    return bounds;
}

RigidBox OrientedBox(Bounds local, const Mat4& world, const BoxCollider& collider) {
    const auto transform = DecomposeTRS(world);
    const auto half = [](float min, float max, float scale) {
        return std::max(MinimumHalfExtent,static_cast<float>((double(max)-min)*std::abs(scale)*0.5));
    };
    return {world.TransformPoint({std::midpoint(local.Min.X,local.Max.X),std::midpoint(local.Min.Y,local.Max.Y),std::midpoint(local.Min.Z,local.Max.Z)}),
        transform.Rotation,{half(local.Min.X,local.Max.X,transform.Scale.X),half(local.Min.Y,local.Max.Y,transform.Scale.Y),
        half(local.Min.Z,local.Max.Z,transform.Scale.Z)},collider.Static ? 0 : collider.Mass,collider.Friction,
        collider.LinearVelocity,collider.AngularVelocity};
}
} // namespace

bool BoxPreview::Matches(const Scene& scene) const {
    return std::ranges::all_of(Sources,[&](const auto& source) {
        if (!scene.Contains(source.Object) || scene.GetObject(source.Object).Model != source.Model ||
            !IsVisible(scene,source.Object) || scene.GetWorldTransform(source.Object).Elements != source.World.Elements) { return false; }
        for (auto parent = scene.GetObject(source.Object).Parent; parent; parent = scene.GetObject(parent).Parent) {
            if (std::ranges::any_of(Sources,[&](const auto& other) {
                return other.Object == parent && other.World.Elements != other.Destination.Elements;
            })) { return false; }
        }
        return true;
    });
}

bool BoxPreview::HasChanges() const {
    return std::ranges::any_of(Sources,[](const auto& source) { return source.World.Elements != source.Destination.Elements; });
}

void BoxPreview::StepRigid(RigidBoxStepOptions options) {
    auto step = StepRigidBoxes(Rigid.Boxes,options);
    for (std::size_t i=0; i<step.Boxes.size(); ++i) {
        const auto& before = InitialRigid[i];
        if (!before.Mass || options.Seconds == 0) { continue; }
        const auto& after = step.Boxes[i];
        auto& source = Sources[i];
        const auto from = before.Position;
        const auto rotation = before.Rotation;
        const auto q = after.Rotation;
        if (q.X == rotation.X && q.Y == rotation.Y && q.Z == rotation.Z && q.W == rotation.W) {
            source.Destination = source.World;
            source.Destination.Elements[12] += after.Position.X-from.X;
            source.Destination.Elements[13] += after.Position.Y-from.Y;
            source.Destination.Elements[14] += after.Position.Z-from.Z;
        } else {
            const auto inverse = Quaternion{-rotation.X,-rotation.Y,-rotation.Z,rotation.W};
            source.Destination = Mat4::Translation(after.Position)*Mat4::Rotation(q)*Mat4::Rotation(inverse)*
                Mat4::Translation({-from.X,-from.Y,-from.Z})*source.World;
        }
    }
    Rigid = std::move(step);
    Seconds += options.Seconds;
}

BoxPreview PreviewBoxes(const Scene& scene, std::span<const BoxCollider> colliders, BoxPreviewMode mode, BoxPreviewOptions options) {
    if (colliders.empty() || colliders.size() > MaxPhysicsBoxes) { throw std::invalid_argument("Add between 1 and 256 colliders."); }
    BoxPreview preview;
    preview.Mode = mode;
    std::unordered_set<ObjectId> dynamic;
    std::unordered_set<ObjectId> added;
    for (const auto& collider : colliders) {
        if (!added.insert(collider.Object).second) { throw std::invalid_argument("An object can only have one box collider."); }
        if (!collider.Static) { dynamic.insert(collider.Object); }
    }
    for (const auto& collider : colliders) {
        const auto& object = scene.GetObject(collider.Object);
        if (!object.Model || !IsVisible(scene,object.Id)) { throw std::invalid_argument("Every collider needs a visible model."); }
        if (mode != BoxPreviewMode::Support) {
            for (auto parent = object.Parent; parent; parent = scene.GetObject(parent).Parent) {
                if (dynamic.contains(parent)) { throw std::invalid_argument("A collider cannot be below a moving collider in the hierarchy."); }
            }
        }
        const auto world = scene.GetWorldTransform(object.Id);
        const auto local = scene.GetModelBounds(object.Id);
        preview.Sources.push_back({object.Id,object.Model,world,world});
        if (mode == BoxPreviewMode::Rigid) { preview.InitialRigid.push_back(OrientedBox(local,world,collider)); }
        else { preview.AxisAligned.push_back({WorldBounds(local,world),collider.Static}); }
    }
    if (mode == BoxPreviewMode::Rigid) {
        preview.Rigid.Boxes = preview.InitialRigid;
        preview.StepRigid({options.Seconds,options.Gravity});
        return preview;
    }
    if (mode == BoxPreviewMode::Gravity) {
        const auto fall = PreviewBoxGravity(preview.AxisAligned,{options.Seconds,options.Acceleration});
        for (std::size_t i=0; i<fall.size(); ++i) {
            const auto change = fall[i].Box.Collider.Min.Y-preview.AxisAligned[i].Collider.Min.Y;
            preview.Sources[i].Destination.Elements[13] += change;
            preview.AxisAligned[i] = fall[i].Box;
            preview.Grounded.push_back(fall[i].InContact);
        }
        preview.Seconds = options.Seconds;
    }
    for (std::size_t i=0; i<preview.AxisAligned.size(); ++i) {
        preview.Support.push_back(CheckBoxSupport(preview.AxisAligned,i,options.SupportTolerance));
    }
    return preview;
}

Result<void> ApplyBoxPreview(EditorDocument& document, const BoxPreview& preview) {
    try {
        if (!preview.Matches(document.GetScene())) {
            return Result<void>::Failure({ErrorCode::InvalidState,"Scene objects changed after the preview.","Run the preview again before applying it.",{}});
        }
    } catch (const std::exception& e) {
        return Result<void>::Failure({ErrorCode::InvalidState,e.what(),"Run the preview again before applying it.",{}});
    }
    if (!preview.HasChanges()) { return Result<void>::Success(); }
    auto result = document.BeginEdit();
    if (!result) { return result; }
    for (const auto& source : preview.Sources) {
        if (source.World.Elements == source.Destination.Elements) { continue; }
        result = document.SetWorldTransform(source.Object,source.Destination);
        if (!result) { return result; } // The document rolls back the active transaction on error.
    }
    return document.CommitEdit();
}
} // namespace lv::editor
