#include <minvex/Scene.h>
#include "core/Utf8.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>
#include <unordered_set>
#include <type_traits>

namespace lv {
namespace {
std::atomic<ObjectId> nextObjectId{1};

bool ValidUserText(std::string_view value, std::size_t maxBytes) noexcept {
    return value.size() <= maxBytes && value.find('\0') == std::string_view::npos && detail::ValidUtf8(value);
}
}

Mat4 Camera::ViewProjection(float aspect) const {
    const auto result=ProjectionMatrix(aspect) * Mat4::LookAt(Position, Target, Up);
    for (auto value:result.Elements) {if (!std::isfinite(value)) {throw std::invalid_argument("Camera matrix exceeds the supported numeric range.");}}return result;
}
Mat4 Camera::ProjectionMatrix(float aspect) const {
    if (!std::isfinite(OrthographicHeight) || OrthographicHeight <= 0 || !std::isfinite(VerticalFovRadians) || VerticalFovRadians <= 0 || VerticalFovRadians >= std::numbers::pi_v<float>) {throw std::invalid_argument("Camera requires a positive orthographic height and 0 < FOV < pi.");}
    if (Projection == CameraProjection::Perspective) {return Mat4::Perspective(VerticalFovRadians,aspect,NearPlane,FarPlane);}
    if (Projection == CameraProjection::Orthographic) {return Mat4::Orthographic(OrthographicHeight*aspect,OrthographicHeight,NearPlane,FarPlane);}
    throw std::invalid_argument("Unknown camera projection.");
}

void DirectionalLight::Validate() const {
    const double length = std::hypot(double(Direction.X), double(Direction.Y), double(Direction.Z));
    if (!std::isfinite(length) || length == 0 || !std::isfinite(Intensity) || Intensity < 0 ||
        !std::isfinite(Ambient) || Ambient < 0 || !std::isfinite(Color.X) || !std::isfinite(Color.Y) ||
        !std::isfinite(Color.Z) || Color.X < 0 || Color.Y < 0 || Color.Z < 0) {
        throw std::invalid_argument("Directional light requires a finite nonzero direction and nonnegative color/intensity/ambient.");
    }
}

ObjectId Scene::CreateObject(std::string name, ObjectId parent) {
    if (parent) { (void)GetObject(parent); }
    const auto id = nextObjectId.fetch_add(1);
    SceneObject object;
    object.Id = id;
    object.Name = std::move(name);
    object.Parent = parent;
    order_.push_back(id);
    try { objects_.emplace(id, std::move(object)); }
    catch (...) { order_.pop_back(); throw; }
    return id;
}

void PointLight::Validate() const {
    if (!std::isfinite(Position.X) || !std::isfinite(Position.Y) || !std::isfinite(Position.Z) || !std::isfinite(Color.X) || !std::isfinite(Color.Y) || !std::isfinite(Color.Z) || Color.X < 0 || Color.Y < 0 || Color.Z < 0 || !std::isfinite(Intensity) || Intensity < 0 || !std::isfinite(Range) || Range <= 0) {throw std::invalid_argument("Local lights require finite position, nonnegative color/intensity and positive range.");}
    Shadow.Validate(Range);
}
void AmbientLight::Validate() const {
    if (!std::isfinite(Intensity)||Intensity<0||!std::isfinite(Color.X)||!std::isfinite(Color.Y)||!std::isfinite(Color.Z)||Color.X<0||Color.Y<0||Color.Z<0) {throw std::invalid_argument("Ambient light requires finite nonnegative color and intensity.");}
}
void LocalShadowSettings::Validate(float range) const {
    if ((Filter!=LocalShadowFilter::Hard&&Filter!=LocalShadowFilter::Pcf)||!std::isfinite(NearPlane)||NearPlane<=0||!std::isfinite(DepthBias)||DepthBias<0||!std::isfinite(SlopeBias)||SlopeBias<0||(Enabled&&NearPlane>=range)) {throw std::invalid_argument("Local shadows require a known filter, 0 < near < range and nonnegative finite world-space biases.");}
}
void SpotLight::Validate() const {
    PointLight{Position,Color,Intensity,Range}.Validate();const auto length=std::hypot(double(Direction.X),double(Direction.Y),double(Direction.Z));
    if (!std::isfinite(length) || length == 0 || !std::isfinite(InnerConeRadians) || !std::isfinite(OuterConeRadians) || InnerConeRadians < 0 || InnerConeRadians > OuterConeRadians || OuterConeRadians <= 0 || OuterConeRadians >= 1.570796327f) {throw std::invalid_argument("Spot lights require nonzero direction and 0 <= inner <= outer < pi/2 (outer > 0).");}
    Shadow.Validate(Range);
}
void RectLight::Validate() const {
    PointLight{Position,Color,Intensity,Range}.Validate();
    const double d=std::hypot(double(Direction.X),double(Direction.Y),double(Direction.Z)),u=std::hypot(double(Up.X),double(Up.Y),double(Up.Z));
    const double x=double(Direction.Y)*Up.Z-double(Direction.Z)*Up.Y,y=double(Direction.Z)*Up.X-double(Direction.X)*Up.Z,z=double(Direction.X)*Up.Y-double(Direction.Y)*Up.X;
    if (!std::isfinite(d)||!std::isfinite(u)||d==0||u==0||std::hypot(x,y,z)<=d*u*1e-6||!std::isfinite(Width)||!std::isfinite(Height)||Width<=0||Height<=0||Width>1e6f||Height>1e6f) {throw std::invalid_argument("Rect lights require independent finite direction/up vectors and dimensions in (0, 1000000].");}
}
void Scene::ValidateLighting() const {
    if (LocalShadowResolution!=128&&LocalShadowResolution!=256&&LocalShadowResolution!=512&&LocalShadowResolution!=1024) {throw std::invalid_argument("Local shadow resolution must be 128, 256, 512 or 1024.");}
    Sun.Validate();Ambient.Validate();if (PointLights.size()+SpotLights.size()+RectLights.size()>MaxLocalLights||RectLights.size()>MaxRectLights) {throw std::invalid_argument("A Scene supports at most 8 local lights, including at most 4 rectangle lights.");}
    std::size_t shadows=0;for (const auto& l:PointLights) {l.Validate();shadows+=l.Shadow.Enabled;}for (const auto& l:SpotLights) {l.Validate();shadows+=l.Shadow.Enabled;}for (const auto& l:RectLights) {l.Validate();}
    if (shadows>MaxShadowedLocalLights) {throw std::invalid_argument("A Scene supports at most 4 shadowed local lights.");}
}

ObjectId Scene::AddModel(ModelHandle model, const Transform& transform, ObjectId parent) {
    if (!model) { throw std::invalid_argument("AddModel requires a nonempty model handle."); }
    (void)transform.Matrix().NormalMatrix();
    AnimationPlayer animation; animation.Bind(model);
    const auto utf8Name = model->SourcePath.stem().generic_u8string();
    const auto id = CreateObject(std::string(utf8Name.begin(), utf8Name.end()), parent);
    auto& object = objects_.at(id);
    object.LocalTransform = transform;
    object.Model = std::move(model);
    object.Animation = std::move(animation);
    return id;
}

const SceneObject& Scene::GetObject(ObjectId id) const {
    const auto found = objects_.find(id);
    if (found == objects_.end()) { throw std::invalid_argument("Object handle does not belong to this Scene or was removed."); }
    return found->second;
}

bool Scene::Contains(ObjectId id) const noexcept { return objects_.contains(id); }

void Scene::SetName(ObjectId id, std::string name) { (void)GetObject(id); objects_.at(id).Name = std::move(name); }

void Scene::SetUserKey(ObjectId id, std::string key) {
    const auto& object = GetObject(id);
    if (!ValidUserText(key, MaxSceneUserKeyBytes)) { throw std::invalid_argument("Invalid Scene user key."); }
    if (object.UserKey == key) { return; }
    if (!key.empty() && !userKeys_.try_emplace(key, id).second) { throw std::invalid_argument("Scene user key already exists."); }
    if (!object.UserKey.empty()) { userKeys_.erase(object.UserKey); }
    objects_.at(id).UserKey = std::move(key);
}

ObjectId Scene::FindObjectByUserKey(std::string_view key) const noexcept {
    const auto found = userKeys_.find(key);
    return found == userKeys_.end() ? NoObject : found->second;
}

void Scene::SetUserMetadata(ObjectId id, std::string key, std::optional<std::string> value) {
    (void)GetObject(id);
    if (key.empty() || !ValidUserText(key, MaxSceneUserKeyBytes) || (value && !ValidUserText(*value, MaxSceneMetadataValueBytes))) {
        throw std::invalid_argument("Invalid Scene metadata key or value.");
    }
    auto& metadata = objects_.at(id).UserMetadata;
    if (!value) { metadata.erase(key); return; }
    if (metadata.size() >= MaxSceneObjectMetadataEntries && !metadata.contains(key)) {
        throw std::length_error("Scene object metadata entry limit exceeded.");
    }
    metadata.insert_or_assign(std::move(key), std::move(*value));
}

void Scene::SetEnvironmentPanorama(EnvironmentPanorama panorama) {
    if (!panorama.Image) { panorama_ = {}; return; }
    const auto& image = *panorama.Image;
    if (image.Width < 2 || image.Width > 8192 || image.Height != image.Width / 2 || image.Width % 2 != 0 ||
        image.Rgba.size() != static_cast<std::size_t>(image.Width) * image.Height * 4 ||
        !std::isfinite(panorama.RotationRadians) || !std::isfinite(panorama.Intensity) ||
        panorama.Intensity < 0 || panorama.Intensity > 16) {
        throw std::invalid_argument("Invalid equirectangular environment panorama.");
    }
    panorama.RotationRadians = std::remainder(panorama.RotationRadians, 2 * std::numbers::pi_v<float>);
    panorama_ = std::move(panorama);
}

void Scene::SetParent(ObjectId id, ObjectId parent) {
    (void)GetObject(id);
    for (auto ancestor = parent; ancestor; ancestor = GetObject(ancestor).Parent) {
        if (ancestor == id) { throw std::invalid_argument("Scene parenting cannot create a cycle."); }
    }
    objects_.at(id).Parent = parent;
    objects_.at(id).AttachmentSocket.clear();
}

void Scene::SetTransform(ObjectId id, const Transform& transform) {
    (void)GetObject(id);
    (void)transform.Matrix().NormalMatrix();
    objects_.at(id).LocalTransform = transform;
}

void Scene::SetVisible(ObjectId id, bool visible) { (void)GetObject(id); objects_.at(id).Visible = visible; }
void Scene::SetModel(ObjectId id, ModelHandle model) {
    ValidateSocketModel(id, model); AnimationPlayer animation; animation.Bind(model);
    const auto& previous = GetObject(id);
    if (!model && previous.SharedSkeleton) { throw std::invalid_argument("Clear Shared Skeleton before detaching its model."); }
    auto binding = previous.SharedSkeleton ? BindSkeleton(*previous.SharedSkeleton, model->Skeleton) : SkeletonBinding{};
    ValidateLeaderBinding(id, binding);
    objects_.at(id).Model = std::move(model); objects_.at(id).Animation = std::move(animation);objects_.at(id).MaterialOverrides.clear();
    objects_.at(id).SkeletonMap = std::move(binding);
}
const MaterialAsset& Scene::GetMaterial(ObjectId id,std::size_t index) const {
    const auto& object=GetObject(id);if (!object.Model || index>=object.Model->Materials.size()) {throw std::invalid_argument("Material index does not belong to the model instance.");}
    const auto found=object.MaterialOverrides.find(index);return found == object.MaterialOverrides.end() ? object.Model->Materials[index]:found->second;
}
void Scene::SetMaterial(ObjectId id,std::size_t index,const MaterialAsset& material) {
    (void)GetMaterial(id,index);ValidateMaterial(material,*GetObject(id).Model);objects_.at(id).MaterialOverrides.insert_or_assign(index,material);
}
void Scene::ResetMaterial(ObjectId id,std::size_t index) {(void)GetMaterial(id,index);objects_.at(id).MaterialOverrides.erase(index);}
AnimationPlayer& Scene::GetAnimation(ObjectId id) {
    if (!GetObject(id).Model) { throw std::invalid_argument("Object has no model animation state."); }
    return objects_.at(id).Animation;
}
void Scene::UpdateAnimations(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) { throw std::invalid_argument("Animation delta must be finite and nonnegative."); }
    for (auto id:order_) {if (objects_.at(id).Model && !objects_.at(id).LeaderPose) {objects_.at(id).Animation.CollectUpdateEvents(deltaSeconds,nullptr);}}
    for (auto id : order_) { if (objects_.at(id).Model && !objects_.at(id).LeaderPose) { objects_.at(id).Animation.Update(deltaSeconds); } }
}
std::vector<SceneAnimationEvent> Scene::PollAnimationEvents() {
    std::vector<SceneAnimationEvent> result;for (auto id:order_) {const auto& player=objects_.at(id).Animation;for (const auto& event:player.PendingAnimationEvents()) {result.push_back({id,event});}}ClearAnimationEventQueues();return result;
}
void Scene::ClearAnimationEventQueues() noexcept {for (auto& [id,object]:objects_) {object.Animation.ClearAnimationEventQueue();}}
std::size_t Scene::DispatchAnimationEvents(std::function<void(const SceneAnimationEvent&)> callback) {
    if (!callback) {throw std::invalid_argument("Event dispatch requires a callback.");}const auto events=PollAnimationEvents();std::exception_ptr failure;
    for (const auto& event:events) {try {callback(event);}catch (...) {if (!failure) {failure=std::current_exception();}}}if (failure) {throw AnimationEventCallbackError(failure);}return events.size();
}

std::size_t Scene::ReplaceModel(ModelHandle previous, ModelHandle replacement) {
    static_assert(std::is_nothrow_move_assignable_v<AnimationPlayer>);
    if (!previous || !replacement) {throw std::invalid_argument("ReplaceModel requires nonempty model handles.");}
    if (previous == replacement) {return 0;}
    struct Replacement { ObjectId Id; AnimationPlayer Player; SkeletonBinding Binding; };
    std::vector<Replacement> pending;
    for (auto id : order_) {
        const auto& object = objects_.at(id);
        if (object.Model != previous) {continue;}
        ValidateSocketModel(id, replacement);
        auto binding = object.SharedSkeleton ? BindSkeleton(*object.SharedSkeleton, replacement->Skeleton) : SkeletonBinding{};
        ValidateLeaderBinding(id, binding);
        if (object.Animation.Model() != previous) {throw std::invalid_argument("Scene player was rebound outside its owning Scene.");}
        for (const auto& [index,material]:object.MaterialOverrides) {
            if (index>=replacement->Materials.size() || previous->Materials.at(index).Name != replacement->Materials[index].Name) {throw std::invalid_argument("Reimport changed an overridden material identity.");}
            for (auto texture:{material.BaseColorTexture,material.NormalTexture,material.MetallicRoughnessTexture}) {
                if (texture && (*texture>=replacement->Textures.size() || previous->Textures.at(*texture).ImageIndex != replacement->Textures[*texture].ImageIndex)) {throw std::invalid_argument("Reimport changed an overridden texture identity.");}
            }
            ValidateMaterial(material,*replacement);
        }
        auto player = object.Animation; player.Rebind(replacement);
        pending.push_back({id,std::move(player),std::move(binding)});
    }
    for (auto& entry : pending) {
        auto& object = objects_.at(entry.Id); object.Model = replacement; object.Animation = std::move(entry.Player); object.SkeletonMap = std::move(entry.Binding);
    }
    return pending.size();
}

Mat4 Scene::GetWorldTransform(ObjectId id) const {
    auto result = Mat4::Identity();
    for (auto current = id; current; current = GetObject(current).Parent) {
        const auto& object = GetObject(current);
        result = AttachmentTransform(object) * object.LocalTransform.Matrix() * result;
    }
    if (!id) { (void)GetObject(id); }
    return result;
}

std::vector<RenderItem> Scene::CollectRenderItems() const {
    PoseCache poses;
    struct State { Mat4 World = Mat4::Identity(); bool Visible = true; };
    std::unordered_map<ObjectId, State> parents;
    std::vector<const SceneObject*> pending;
    std::vector<RenderItem> result;
    const auto evaluate = [&](State state, const SceneObject& object) {
        const auto local = object.LocalTransform.Matrix();
        if (object.AttachmentSocket.empty()) {
            state.World = object.Parent ? state.World * local : local;
        } else {
            state.World = state.World * AttachmentTransform(object, &poses) * local;
        }
        state.Visible = state.Visible && object.Visible;
        return state;
    };
    // Iterative parent-chain evaluation works even after reparenting to an object
    // created later. Store only ancestors used by multiple evaluations.
    for (const auto id : order_) {
        const auto& object = GetObject(id);
        State state;
        if (const auto cached = parents.find(id); cached != parents.end()) {
            state = cached->second;
        } else {
            for (auto parent = object.Parent; parent;) {
                if (const auto ancestor = parents.find(parent); ancestor != parents.end()) {
                    state = ancestor->second;
                    break;
                }
                pending.push_back(&GetObject(parent));
                parent = pending.back()->Parent;
            }
            for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
                state = evaluate(state, **it);
                parents.emplace((*it)->Id, state);
            }
            pending.clear();
            state = evaluate(state, object);
        }
        if (object.Model && object.Animation.Model() != object.Model) { throw std::invalid_argument("Scene-owned player was rebound; use Scene::SetModel."); }
        if (state.Visible && object.Model) {
            const auto pose = object.LeaderPose || object.Animation.NeedsEvaluation() ? ResolvePose(id, poses) : nullptr;
            result.push_back({id, object.Model, state.World,&object.Animation,&object.MaterialOverrides,pose});
        }
    }
    return result;
}

void Scene::RemoveObject(ObjectId id) {
    (void)GetObject(id);
    std::unordered_set<ObjectId> removed{id};
    // Each object's ancestry is evaluated once via the same topological traversal.
    std::unordered_map<ObjectId, bool> membership;
    for (const auto candidate : order_) {
        std::vector<ObjectId> chain;
        auto current = candidate;
        while (current && !membership.contains(current) && current != id) {
            chain.push_back(current);
            current = GetObject(current).Parent;
        }
        const bool inSubtree = current == id || (current && membership.at(current));
        for (auto ancestor : chain) { membership[ancestor] = inSubtree; }
        if (inSubtree) { removed.insert(candidate); }
    }
    for (const auto removedId : removed) {
        EraseParticleEffect(removedId);
        const auto& key = objects_.at(removedId).UserKey;
        if (!key.empty()) { userKeys_.erase(key); }
        objects_.erase(removedId);
    }
    for (auto& [remainingId, object] : objects_) {
        (void)remainingId;
        if (removed.contains(object.LeaderPose)) { object.LeaderPose = NoObject; object.Animation.ClearAnimationEventQueue(); }
    }
    std::erase_if(order_, [&removed](ObjectId candidate) { return removed.contains(candidate); });
}

} // namespace lv
