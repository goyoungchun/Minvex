#pragma once

#include <minvex/Debug.h>
#include <sokol_gfx.h>

#include <optional>
#include <unordered_map>

namespace lv::detail {

class ParticleComputeRenderer;

class ParticleRenderer final {
public:
    ParticleRenderer();
    ~ParticleRenderer();
    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    void Prepare(const Scene& scene, const RenderSettings& settings, std::optional<float> cullAspect);
    std::size_t Draw(const Mat4& projection);
    std::size_t Count() const noexcept;
    std::size_t Triangles() const noexcept { return triangles_; }
    std::size_t BufferBytes() const noexcept;
    std::size_t TextureBytes() const noexcept;
    void Inspect(std::vector<DebugResource>& resources) const;

private:
    struct Instance {
        std::array<float, 4> CenterSize;
        std::array<float, 2> RotationAspect;
        std::array<std::uint8_t, 4> Color;
        float Frame;
    };
    static_assert(sizeof(Instance) == 32);
    struct Material {
        const ParticleTexture* Texture = nullptr;
        std::uint16_t Columns = 1, Rows = 1;
        ParticleBlend Blend = ParticleBlend::Alpha;
        ParticleRendererType Geometry = ParticleRendererType::Sprite;
        const ModelAsset* Mesh = nullptr;
        bool operator==(const Material&) const = default;
    };
    struct Run { Material Surface; std::size_t First, Count; };
    struct GpuMesh {
        ModelHandle Source;
        sg_buffer Vertices{};
        int Count = 0;
        bool Used = false;
        explicit GpuMesh(ModelHandle source);
        ~GpuMesh();
    };
    struct GpuTexture {
        ParticleTextureHandle Source;
        sg_image Image{};
        sg_view View{};
        bool Used = false;
        explicit GpuTexture(ParticleTextureHandle source);
        ~GpuTexture();
    };
    struct AlphaOrder { float Depth; std::uint32_t Index; };
    struct Stream { sg_buffer Buffer{}; std::size_t Bytes = 0; };

    void Initialize(const RenderSettings& settings);
    void Upload();
    void PrepareResources();
    void Destroy() noexcept;
    static void OnCommit(void* user) noexcept;

    sg_shader shader_{};
    sg_pipeline alphaPipeline_{}, additivePipeline_{};
    sg_pipeline meshAlpha_{}, meshAdditive_{};
    sg_buffer corners_{};
    sg_sampler sampler_{};
    std::unique_ptr<GpuTexture> fallback_;
    std::unordered_map<const ParticleTexture*, std::unique_ptr<GpuTexture>> textures_;
    std::unordered_map<const ModelAsset*, std::unique_ptr<GpuMesh>> meshes_;
    bool listening_ = false;
    std::vector<Stream> streams_;
    std::size_t usedStreams_ = 0, preparedStream_ = 0;
    std::size_t triangles_ = 0;
    Mat4 view_;
    std::vector<Run> runs_;
    std::vector<ParticleSprite> sprites_;
    std::vector<Instance> instances_;
    std::vector<AlphaOrder> sortedAlpha_;
#if !defined(__EMSCRIPTEN__)
    std::unique_ptr<ParticleComputeRenderer> compute_;
    std::vector<ParticleGpuSprite> gpuSprites_;
#endif
};

} // namespace lv::detail
