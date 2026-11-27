#pragma once

#include <minvex/Asset.h>
#include <minvex/Particles.h>
#include <minvex/Skeleton.h>

#include <limits>
#include <map>
#include <unordered_map>

namespace lv {

class Scene;
namespace detail {
struct SceneParticles;
class ParticleRenderer;
std::uint64_t SceneRenderSignature(const Scene&, std::uint64_t geometry);
}

using ObjectId = std::uint64_t;
inline constexpr ObjectId NoObject = 0;
struct EnvironmentPanorama {
    std::shared_ptr<const ImageAsset> Image; // Immutable 2:1 equirectangular sRGB RGBA8, top row is +Y.
    float RotationRadians = 0;
    float Intensity = 1; // Linear RGB multiplier in [0,16].
};
struct SceneAnimationEvent { ObjectId Object = NoObject; AnimationEventOccurrence Occurrence; };
struct SceneParticleEvent {
    ObjectId Object = NoObject;
    std::size_t Emitter = 0;
    ParticleEventKind Kind = ParticleEventKind::Birth;
    std::uint32_t Sequence = 0;
    double Time = 0; // Seconds on the effect clock.
    Vec3 Position{}; // World space.
};
struct ParticleEventBatch {
    std::vector<SceneParticleEvent> Events;
    std::size_t Dropped = 0;
};

enum class ParticleRenderPath { None, Cpu, Gpu };
enum class ParticleGpuFallback { None, Renderer, Blend, Collision, Trail, Texture, ComputeUnavailable };
struct ParticleEmitterDebugInfo {
    ObjectId Object = NoObject;
    std::size_t Emitter = 0; // Index in the bound effect.
    ParticleSimulation Requested = ParticleSimulation::Cpu;
    ParticleRenderPath Path = ParticleRenderPath::None;
    ParticleGpuFallback Fallback = ParticleGpuFallback::None;
    std::size_t SubmittedParticles = 0; // Distinct particles submitted for draw.
};


enum class CameraProjection { Perspective, Orthographic };
struct Camera {
    Vec3 Position{0, 1.5f, 4};
    Vec3 Target{};
    Vec3 Up{0, 1, 0};
    float VerticalFovRadians = 1.04719755f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    CameraProjection Projection = CameraProjection::Perspective;
    float OrthographicHeight = 10.0f; // Full vertical span in world units.
    Mat4 ProjectionMatrix(float aspect) const;
    Mat4 ViewProjection(float aspect) const;
};

// Screen coordinates use a top-left origin. ScreenPoint::Depth is the [0,1]
// depth after projection (0 at the near plane). X and Y may lie outside the viewport.
struct ScreenPoint { float X = 0, Y = 0; double Depth = 0; };
struct ViewportSize { float Width = 0, Height = 0; };
struct Ray {
    Vec3 Origin{};
    Vec3 Direction{0, 0, -1};
    double MinDistance = 0, MaxDistance = std::numeric_limits<double>::infinity();
};
ScreenPoint WorldToScreen(const Camera& camera, ViewportSize viewport, Vec3 world);
Vec3 ScreenToWorld(const Camera& camera, ViewportSize viewport, ScreenPoint point);
Ray ScreenRay(const Camera& camera, ViewportSize viewport, float x, float y);

struct SurfaceHit {
    ObjectId Object = NoObject;
    Vec3 Position{};
    double Distance = 0;
    std::size_t Part = 0, Primitive = 0, Triangle = 0;
    std::array<double, 3> Barycentric{}; // Index order within the source triangle.
};

struct DirectionalLight {
    Vec3 Direction{-0.4f, -0.8f, -0.6f}; // Direction in which light travels.
    Vec3 Color{1, 1, 1};
    float Intensity = 0.75f;
    float Ambient = 0.25f;
    void Validate() const;
};
inline constexpr std::size_t MaxLocalLights = 8;
inline constexpr std::size_t MaxRectLights = 4, MaxShadowedLocalLights = 4;
struct AmbientLight {
    Vec3 Color{1,1,1};
    float Intensity=0; // Added to legacy Sun.Ambient; linear RGB, no occlusion/IBL.
    void Validate() const;
};
enum class LocalShadowFilter { Hard, Pcf };
struct LocalShadowSettings {
    bool Enabled=false;
    LocalShadowFilter Filter=LocalShadowFilter::Pcf;
    float NearPlane=.05f, DepthBias=.005f, SlopeBias=.02f; // World-space bias.
    void Validate(float range) const;
};
struct PointLight {
    Vec3 Position{}, Color{1,1,1};
    float Intensity = 1, Range = 10;
    LocalShadowSettings Shadow{};
    void Validate() const;
};
struct SpotLight {
    Vec3 Position{}, Direction{0,-1,0}, Color{1,1,1};
    float Intensity = 1, Range = 10;
    float InnerConeRadians = .35f, OuterConeRadians = .7f;
    LocalShadowSettings Shadow{};
    void Validate() const;
};
struct RectLight {
    Vec3 Position{},Direction{0,-1,0},Up{0,0,1},Color{1,1,1};
    float Intensity=1,Width=2,Height=2,Range=10; // Radiance scale; full dimensions.
    bool TwoSided=false; // LTC area illumination; no Rect shadow mapping.
    void Validate() const;
};

inline constexpr std::size_t MaxSceneUserKeyBytes = 128;
inline constexpr std::size_t MaxSceneMetadataValueBytes = 4096;
inline constexpr std::size_t MaxSceneObjectMetadataEntries = 32;

struct SceneObject {
    ObjectId Id = NoObject;
    std::string Name;
    ObjectId Parent = NoObject;
    Transform LocalTransform;
    bool Visible = true;
    ModelHandle Model;
    AnimationPlayer Animation;
    std::unordered_map<std::size_t,MaterialAsset> MaterialOverrides;
    std::vector<SkeletonSocket> Sockets;
    std::string AttachmentSocket; // Empty: ordinary parenting. Otherwise uses Parent's socket.
    SharedSkeletonHandle SharedSkeleton;
    SkeletonBinding SkeletonMap;
    ObjectId LeaderPose = NoObject; // Independent from transform parenting.
    std::string UserKey;
    std::map<std::string, std::string, std::less<>> UserMetadata;
};

struct RenderItem {
    ObjectId Object = NoObject;
    ModelHandle Model;
    Mat4 WorldTransform;
    const AnimationPlayer* Animation = nullptr; // Valid until the owning Scene object is removed.
    const std::unordered_map<std::size_t,MaterialAsset>* MaterialOverrides = nullptr;
    std::shared_ptr<const EvaluatedPose> Pose; // Owned snapshot, model-node index order. Shared during collection.
};

class Scene {
public:
    Scene();
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept;
    Scene& operator=(Scene&&) noexcept;

    ObjectId CreateObject(std::string name = {}, ObjectId parent = NoObject);
    ObjectId AddModel(ModelHandle model, const Transform& transform = {}, ObjectId parent = NoObject);
    void RemoveObject(ObjectId id); // Removes the entire subtree.
    const SceneObject& GetObject(ObjectId id) const;
    bool Contains(ObjectId id) const noexcept;
    std::size_t Size() const noexcept { return objects_.size(); }
    std::vector<ObjectId> ObjectIds() const { return order_; } // Creation order, including empty/hidden objects.
    void SetName(ObjectId id, std::string name);
    void SetUserKey(ObjectId id, std::string key); // Scene-unique, empty clears.
    ObjectId FindObjectByUserKey(std::string_view key) const noexcept; // NoObject on miss.
    void SetUserMetadata(ObjectId id, std::string key, std::optional<std::string> value); // nullopt erases.
    void SetEnvironmentPanorama(EnvironmentPanorama panorama); // Empty image clears the runtime background.
    const EnvironmentPanorama& GetEnvironmentPanorama() const noexcept { return panorama_; }
    void SetParent(ObjectId id, ObjectId parent); // Keeps local TRS; clears socket attachment; rejects cycles.
    void SetTransform(ObjectId id, const Transform& transform);
    void SetWorldTransform(ObjectId id, const Mat4& world); // Includes socket frame; rejects local shear.
    void SetVisible(ObjectId id, bool visible);
    void SetModel(ObjectId id, ModelHandle model); // Empty handle detaches the model.
    void SetSocket(ObjectId id, const SkeletonSocket& socket); // Per-instance definition/override.
    void RemoveSocket(ObjectId id, std::string_view name); // Removes instance override; requires fallback or no attachments.
    const SkeletonSocket& GetSocket(ObjectId id, std::string_view name) const;
    Mat4 GetSocketWorldTransform(ObjectId id, std::string_view name) const;
    ObjectId CreateAttachmentNode(ObjectId parent, std::string socket, const Transform& relative = {}, std::string name = {});
    void AttachToSocket(ObjectId id, ObjectId parent, std::string socket, const Transform& relative = {});
    void DetachFromSocket(ObjectId id, bool keepWorld = true); // To scene root; shear cannot be kept as TRS.
    EvaluatedPose EvaluatePose(ObjectId id) const; // Final scene-resolved model-space pose.
    Bounds GetModelBounds(ObjectId id) const; // Final resolved pose plus instance morph weights.
    void SetSharedSkeleton(ObjectId id, SharedSkeletonHandle skeleton);
    void SetLeaderPose(ObjectId follower, ObjectId leader); // NoObject restores dormant independent player.
    const MaterialAsset& GetMaterial(ObjectId id, std::size_t index) const;
    void SetMaterial(ObjectId id, std::size_t index, const MaterialAsset& material); // Per-instance override.
    void ResetMaterial(ObjectId id, std::size_t index);
    std::size_t ReplaceModel(ModelHandle previous, ModelHandle replacement); // All matching instances, transactional; preserves compatible animation/pose.
    AnimationPlayer& GetAnimation(ObjectId id); // Model instance state; throws for empty objects.
    void UpdateAnimations(double deltaSeconds);
    std::vector<SceneAnimationEvent> PollAnimationEvents(); // Scene order, then each player's traversal order.
    std::size_t DispatchAnimationEvents(std::function<void(const SceneAnimationEvent&)> callback);
    void ClearAnimationEventQueues() noexcept;
    // An empty handle detaches the effect. Attaching starts a fresh seeded instance.
    void SetParticleEffect(ObjectId id, ParticleEffectHandle effect, std::uint32_t seed = 0);
    ParticleEffectBinding GetParticleEffect(ObjectId id) const;
    void RestartParticles(ObjectId id);
    void SeekParticles(ObjectId id, double seconds); // Replays with the object's current transform.
    double ParticleTime(ObjectId id) const;
    void SetParticleEmission(ObjectId id, bool enabled); // Pauses emission, lets existing particles age.
    // Instance-local gates. Existing particles continue updating. A disabled emitter
    // receives neither scheduled nor routed births. The effect definition stays immutable.
    void SetParticleEmitterEmission(ObjectId id, std::size_t emitter, bool enabled);
    // Gate outgoing routes for this event kind on the source emitter. Published events
    // and the source's own particles remain active. Events missed while off are discarded.
    void SetParticleEventRoutesEnabled(ObjectId id, std::size_t source, ParticleEventKind event, bool enabled);
    void UpdateParticles(double deltaSeconds);
    ParticleEventBatch PollParticleEvents();
    bool HasParticleEffects() const noexcept { return bool(particles_); }
    std::size_t ParticleCount(ObjectId id = NoObject) const; // Includes hidden particles.
    void CollectParticleSprites(std::vector<ParticleSprite>& sprites,
                                std::vector<ParticleGpuSprite>* gpu = nullptr) const;
    Mat4 GetWorldTransform(ObjectId id) const;
    std::vector<RenderItem> CollectRenderItems() const;
    // Closest two-sided LOD0 triangle, including the current pose and morph weights.
    // A nonzero object filter matches exactly one visible instance.
    std::optional<SurfaceHit> PickSurface(const Ray& ray, ObjectId object = NoObject) const;

    Camera MainCamera;
    DirectionalLight Sun;
    AmbientLight Ambient;
    int LocalShadowResolution=512; // Per face, shared atlas; 128/256/512/1024.
    std::vector<PointLight> PointLights;
    std::vector<SpotLight> SpotLights;
    std::vector<RectLight> RectLights;
    void ValidateLighting() const; // <=8 local, <=4 rect, <=4 shadowed point/spot.

private:
    friend class EditorDocument; // Private CPU history snapshots preserve object identities.
    friend class detail::ParticleRenderer;
    friend std::uint64_t detail::SceneRenderSignature(const Scene&, std::uint64_t);
    struct ParticleRenderTag { std::size_t Emitter; std::uint32_t Sequence; };
    void CollectParticleRenderData(std::vector<ParticleSprite>& sprites, std::vector<ParticleGpuSprite>* gpu,
                                   std::vector<ParticleEmitterDebugInfo>* diagnostics, std::vector<ParticleRenderTag>* tags) const;
    std::unordered_map<ObjectId, SceneObject> objects_;
    std::map<std::string, ObjectId, std::less<>> userKeys_;
    EnvironmentPanorama panorama_;
    std::vector<ObjectId> order_;
    std::unique_ptr<detail::SceneParticles> particles_;
    std::uint64_t ParticleRenderSignature() const;
    bool ParticleObjectVisible(ObjectId id) const;
    void EraseParticleEffect(ObjectId id) noexcept;
    void CopyParticleEffectsTo(Scene& snapshot) const;
    void ReuseParticlePreviewsFrom(Scene& previous) noexcept;
    using PoseCache = std::unordered_map<ObjectId, std::shared_ptr<const EvaluatedPose>>;
    std::shared_ptr<const EvaluatedPose> ResolvePose(ObjectId id, PoseCache& cache) const;
    Mat4 AttachmentTransform(const SceneObject& object, PoseCache* cache = nullptr) const;
    void ValidateSocketModel(ObjectId id, const ModelHandle& model) const;
    void ValidateLeaderBinding(ObjectId id, const SkeletonBinding& binding) const;
};

} // namespace lv
