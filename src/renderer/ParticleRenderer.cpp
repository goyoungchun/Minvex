#include "ParticleRenderer.h"
#include "DebugResources.h"
#include "particles.glsl.h"
#include "platform/GpuResourceError.h"
#if !defined(__EMSCRIPTEN__)
#include "ParticleComputeRenderer.h"
#endif

#include <algorithm>
#include <bit>
#include <cmath>
#include <numeric>

namespace lv::detail {

ParticleRenderer::GpuTexture::GpuTexture(ParticleTextureHandle source) : Source(std::move(source)) {
    constexpr std::array<std::uint8_t, 4> white{255, 255, 255, 255};
    try {
        sg_image_desc desc{};
        desc.width = Source ? Source->Width() : 1;
        desc.height = Source ? Source->Height() : 1;
        desc.pixel_format = Source ? SG_PIXELFORMAT_SRGB8A8 : SG_PIXELFORMAT_RGBA8;
        desc.data = Source ? sg_range{Source->Pixels().data(), Source->Pixels().size()} : SG_RANGE(white);
        desc.label = "particle-flipbook";
        Image = sg_make_image(desc);
        RequireGpuResource(sg_query_image_state(Image), Image.id, ImagePool, "particle sprite texture");
        sg_view_desc view{};
        view.texture.image = Image;
        View = sg_make_view(view);
        RequireGpuResource(sg_query_view_state(View), View.id, ViewPool, "particle sprite texture view");
    } catch (...) {
        sg_destroy_view(View);
        sg_destroy_image(Image);
        throw;
    }
}

ParticleRenderer::GpuTexture::~GpuTexture() {
    sg_destroy_view(View);
    sg_destroy_image(Image);
}

ParticleRenderer::GpuMesh::GpuMesh(ModelHandle source) : Source(std::move(source)) {
    std::vector<Vec3> vertices;
    const auto append = [&](const MeshData& mesh, const Mat4& transform) {
        for (const auto index : mesh.Indices) {
            const auto& v = mesh.Vertices[index].Position;
            vertices.push_back(transform.TransformPoint({v[0], v[1], v[2]}));
        }
    };
    if (Source) {
        for (const auto& part : Source->Parts) {
            for (const auto& primitive : Source->Meshes[part.MeshIndex].Primitives) { append(primitive.Geometry, part.Transform); }
        }
    } else { append(MakeCube(), Mat4::Identity()); }
    Count = static_cast<int>(vertices.size());
    sg_buffer_desc desc{};
    desc.data = {vertices.data(), vertices.size() * sizeof(Vec3)};
    desc.label = "particle-mesh-vertices";
    Vertices = sg_make_buffer(desc);
    try { RequireGpuResource(sg_query_buffer_state(Vertices), Vertices.id, BufferPool, "particle mesh"); }
    catch (...) { sg_destroy_buffer(Vertices); throw; }
}

ParticleRenderer::GpuMesh::~GpuMesh() { sg_destroy_buffer(Vertices); }

ParticleRenderer::ParticleRenderer() = default;
ParticleRenderer::~ParticleRenderer() { Destroy(); }

std::size_t ParticleRenderer::Count() const noexcept {
#if !defined(__EMSCRIPTEN__)
    if (compute_) { return instances_.size() + compute_->Count(); }
#endif
    return instances_.size();
}

void ParticleRenderer::Destroy() noexcept {
    if (listening_) { sg_remove_commit_listener({OnCommit, this}); listening_ = false; }
    for (auto stream : streams_) { sg_destroy_buffer(stream.Buffer); }
    streams_.clear();
    textures_.clear();
    meshes_.clear();
    fallback_.reset();
    sg_destroy_sampler(sampler_);
    sg_destroy_buffer(corners_);
    sg_destroy_pipeline(additivePipeline_);
    sg_destroy_pipeline(alphaPipeline_);
    sg_destroy_pipeline(meshAlpha_);
    sg_destroy_pipeline(meshAdditive_);
    sg_destroy_shader(shader_);
    corners_ = {};
    sampler_ = {};
    alphaPipeline_ = additivePipeline_ = {};
    meshAlpha_ = meshAdditive_ = {};
    shader_ = {};
}

void ParticleRenderer::OnCommit(void* user) noexcept {
    auto& self = *static_cast<ParticleRenderer*>(user);
    self.usedStreams_ = 0;
    self.instances_.clear();
    self.triangles_ = 0;
}

void ParticleRenderer::Initialize(const RenderSettings& settings) {
    if (shader_.id) { return; }
    try {
        shader_ = sg_make_shader(particle_sprite_shader_desc(sg_query_backend()));
        RequireGpuResource(sg_query_shader_state(shader_), shader_.id, ShaderPool, "particle sprite shader");
        constexpr float corners[][3] = {{-1, -1, 0}, {1, -1, 0}, {-1, 1, 0}, {1, 1, 0}};
        sg_buffer_desc cornersDesc{};
        cornersDesc.data = SG_RANGE(corners);
        cornersDesc.label = "particle-billboard-corners";
        corners_ = sg_make_buffer(cornersDesc);
        RequireGpuResource(sg_query_buffer_state(corners_), corners_.id, BufferPool, "particle billboard corners");
        fallback_ = std::make_unique<GpuTexture>(nullptr);
        sg_sampler_desc sampler{};
        sampler.min_filter = sampler.mag_filter = SG_FILTER_LINEAR;
        sampler.wrap_u = sampler.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        sampler_ = sg_make_sampler(sampler);
        RequireGpuResource(sg_query_sampler_state(sampler_), sampler_.id, SamplerPool, "particle sprite sampler");

        sg_pipeline_desc pipeline{};
        pipeline.shader = shader_;
        pipeline.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
        pipeline.sample_count = settings.MsaaSamples;
        pipeline.colors[0].pixel_format = settings.PostProcessing ? SG_PIXELFORMAT_RGBA16F :
            sg_query_desc().environment.defaults.color_format;
        pipeline.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pipeline.depth.write_enabled = false;
        pipeline.layout.buffers[0].stride = 3 * sizeof(float);
        pipeline.layout.buffers[1].stride = sizeof(Instance);
        pipeline.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
        pipeline.layout.attrs[ATTR_particle_sprite_corner] = {0, 0, SG_VERTEXFORMAT_FLOAT3};
        pipeline.layout.attrs[ATTR_particle_sprite_center_size] = {1, offsetof(Instance, CenterSize), SG_VERTEXFORMAT_FLOAT4};
        pipeline.layout.attrs[ATTR_particle_sprite_rotation_aspect] = {1, offsetof(Instance, RotationAspect), SG_VERTEXFORMAT_FLOAT2};
        pipeline.layout.attrs[ATTR_particle_sprite_color0] = {1, offsetof(Instance, Color), SG_VERTEXFORMAT_UBYTE4N};
        pipeline.layout.attrs[ATTR_particle_sprite_frame_index] = {1, offsetof(Instance, Frame), SG_VERTEXFORMAT_FLOAT};
        auto& blend = pipeline.colors[0].blend;
        blend.enabled = true;
        blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
        blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pipeline.label = "particle-alpha-pipeline";
        alphaPipeline_ = sg_make_pipeline(pipeline);
        RequireGpuResource(sg_query_pipeline_state(alphaPipeline_), alphaPipeline_.id, PipelinePool, "particle alpha pipeline");
        blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        blend.src_factor_alpha = SG_BLENDFACTOR_ZERO;
        blend.dst_factor_alpha = SG_BLENDFACTOR_ONE;
        pipeline.label = "particle-additive-pipeline";
        additivePipeline_ = sg_make_pipeline(pipeline);
        RequireGpuResource(sg_query_pipeline_state(additivePipeline_), additivePipeline_.id, PipelinePool, "particle additive pipeline");
        listening_ = sg_add_commit_listener({OnCommit, this});
        if (!listening_) { throw std::runtime_error("Cannot register the particle buffer commit listener."); }
    } catch (...) { Destroy(); throw; }
}

void ParticleRenderer::Prepare(const Scene& scene, const RenderSettings& settings, std::optional<float> cullAspect) {
    instances_.clear();
    triangles_ = 0;
    runs_.clear();
    sortedAlpha_.clear();
    const auto& camera = scene.MainCamera;
#if !defined(__EMSCRIPTEN__)
    auto* gpu = sg_query_features().compute ? &gpuSprites_ : nullptr;
#else
    std::vector<ParticleGpuSprite>* gpu = nullptr;
#endif
    scene.CollectParticleRenderData(sprites_, gpu, &diagnostics_, &tags_);
#if !defined(__EMSCRIPTEN__)
    if (gpu && !gpu->empty()) {
        if (!compute_) { compute_ = std::make_unique<ParticleComputeRenderer>(settings); }
        compute_->Prepare(*gpu, Mat4::LookAt(camera.Position, camera.Target, camera.Up));
    } else if (compute_) { compute_->Prepare({}, Mat4::Identity()); }
    if (compute_) { triangles_ = 2 * compute_->Count(); }
#endif
    if (sprites_.empty()) { textures_.clear(); meshes_.clear(); return; }

    view_ = Mat4::LookAt(camera.Position, camera.Target, camera.Up);
    const auto projection = cullAspect ? camera.ProjectionMatrix(*cullAspect) : Mat4::Identity();
    const auto append = [&](const ParticleSprite& sprite, Vec3 center) {
        const bool line = sprite.Renderer == ParticleRendererType::Ribbon || sprite.Renderer == ParticleRendererType::Beam;
        Instance instance{{center.X, center.Y, center.Z, sprite.Size}, {sprite.Rotation, sprite.AspectRatio}, {},
                          line ? sprite.TrailEnd->Z : static_cast<float>(sprite.Frame)};
        for (std::size_t c = 0; c < instance.Color.size(); ++c) {
            instance.Color[c] = static_cast<std::uint8_t>(sprite.Color[c] * 255 + 0.5f);
        }
        const bool mesh = sprite.Renderer == ParticleRendererType::Mesh;
        const Material material{mesh ? nullptr : sprite.Texture, sprite.Columns, sprite.Rows, sprite.Blend,
                                sprite.Renderer, sprite.Mesh ? sprite.Mesh->get() : nullptr};
        if (mesh && !meshes_.contains(material.Mesh)) {
            meshes_.emplace(material.Mesh, std::make_unique<GpuMesh>(sprite.Mesh ? *sprite.Mesh : ModelHandle{}));
        }
        if (runs_.empty() || runs_.back().Surface != material) { runs_.push_back({material, instances_.size(), 0}); }
        ++runs_.back().Count;
        instances_.push_back(instance);
    };
    std::optional<Scene::ParticleRenderTag> lastSubmitted;
    for (std::uint32_t i = 0; i < sprites_.size(); ++i) {
        auto& sprite = sprites_[i];
        if (sprite.Color[3] * 255 + 0.5f < 1) { continue; }
        auto center = view_.TransformPoint(sprite.Position);
        if (sprite.TrailEnd) {
            const auto end = view_.TransformPoint(*sprite.TrailEnd);
            const float x = end.X - center.X, y = end.Y - center.Y;
            const float length = std::hypot(x, y);
            if (!std::isfinite(length) || length <= 1e-6f) { continue; }
            sprite.Rotation = std::atan2(-x, y);
            sprite.AspectRatio = (length + (sprite.Renderer == ParticleRendererType::Sprite ? 0.8f * sprite.Size : 0)) / sprite.Size;
            if (!std::isfinite(sprite.AspectRatio)) { continue; }
            sprite.TrailEnd->Z = end.Z - center.Z;
            center = {std::midpoint(center.X, end.X), std::midpoint(center.Y, end.Y), std::midpoint(center.Z, end.Z)};
        }
        if (!std::isfinite(center.X) || !std::isfinite(center.Y) || !std::isfinite(center.Z)) { continue; }
        const float distance = sprite.MaxDrawDistance;
        if (distance > 0 && center.X * center.X + center.Y * center.Y + center.Z * center.Z > distance * distance) { continue; }
        if (settings.FrustumCulling && sprite.Renderer == ParticleRendererType::Sprite) {
            if (-center.Z < camera.NearPlane || -center.Z > camera.FarPlane) { continue; }
            if (cullAspect) {
                const float radius = 0.5f * sprite.Size * std::sqrt(1.0f + sprite.AspectRatio * sprite.AspectRatio);
                const float w = camera.Projection == CameraProjection::Perspective ? -center.Z : 1.0f;
                if ((std::abs(center.X) - radius) * projection.Elements[0] > w ||
                    (std::abs(center.Y) - radius) * projection.Elements[5] > w) { continue; }
            }
        }
        // Trail pieces follow their parent in the collected stream. Count a
        // surviving head or trail once, before alpha sorting reorders the stream.
        const auto tag = tags_[i];
        if (!lastSubmitted || lastSubmitted->Emitter != tag.Emitter || lastSubmitted->Sequence != tag.Sequence) {
            auto& diagnostic = diagnostics_[tag.Emitter];
            diagnostic.Path = ParticleRenderPath::Cpu;
            ++diagnostic.SubmittedParticles;
            lastSubmitted = tag;
        }
        if (sprite.Blend == ParticleBlend::Additive) { append(sprite, center); }
        else {
            sprite.Position = center;
            sortedAlpha_.push_back({center.Z, i});
        }
    }
    std::sort(sortedAlpha_.begin(), sortedAlpha_.end(), [](const auto& a, const auto& b) {
        return a.Depth == b.Depth ? a.Index < b.Index : a.Depth < b.Depth;
    });
    const auto required = instances_.size() + sortedAlpha_.size();
    if (instances_.capacity() < required) { instances_.reserve(std::bit_ceil(required)); }
    for (const auto& entry : sortedAlpha_) {
        const auto& sprite = sprites_[entry.Index];
        append(sprite, sprite.Position);
    }
    if (!instances_.empty()) {
        Initialize(settings);
        PrepareResources();
        Upload();
    } else { textures_.clear(); meshes_.clear(); }
}

void ParticleRenderer::PrepareResources() {
    for (auto& [texture, gpu] : textures_) { (void)texture; gpu->Used = false; }
    for (auto& [mesh, gpu] : meshes_) { (void)mesh; gpu->Used = false; }
    for (const auto& run : runs_) {
        if (run.Surface.Geometry == ParticleRendererType::Mesh) {
            auto& mesh = *meshes_.at(run.Surface.Mesh);
            mesh.Used = true;
            triangles_ += run.Count * static_cast<std::size_t>(mesh.Count / 3);
            if (!meshAlpha_.id) {
                auto pipeline = sg_query_pipeline_desc(alphaPipeline_);
                pipeline.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
                pipeline.label = "particle-mesh-alpha";
                const auto alpha = sg_make_pipeline(pipeline);
                sg_pipeline additive{};
                try {
                    RequireGpuResource(sg_query_pipeline_state(alpha), alpha.id, PipelinePool, "particle mesh alpha pipeline");
                    pipeline.colors[0].blend = sg_query_pipeline_desc(additivePipeline_).colors[0].blend;
                    pipeline.label = "particle-mesh-additive";
                    additive = sg_make_pipeline(pipeline);
                    RequireGpuResource(sg_query_pipeline_state(additive), additive.id, PipelinePool, "particle mesh additive pipeline");
                } catch (...) {
                    sg_destroy_pipeline(alpha);
                    sg_destroy_pipeline(additive);
                    throw;
                }
                meshAlpha_ = alpha;
                meshAdditive_ = additive;
            }
        } else { triangles_ += 2 * run.Count; }
        const auto* texture = run.Surface.Texture;
        if (!texture) { continue; }
        const auto found = textures_.find(texture);
        if (found != textures_.end()) { found->second->Used = true; }
        else {
            auto gpu = std::make_unique<GpuTexture>(texture->shared_from_this());
            gpu->Used = true;
            textures_.emplace(texture, std::move(gpu));
        }
    }
    std::erase_if(textures_, [](const auto& entry) { return !entry.second->Used; });
    std::erase_if(meshes_, [](const auto& entry) { return !entry.second->Used; });
}

void ParticleRenderer::Upload() {
    // A transient buffer can be rewritten until its first bind. Use another
    // stream if the host submits a second view before sg_commit().
    preparedStream_ = usedStreams_;
    if (preparedStream_ == streams_.size()) { streams_.emplace_back(); }
    auto& stream = streams_[preparedStream_];
    const auto bytes = instances_.size() * sizeof(Instance);
    if (stream.Bytes < bytes) {
        sg_buffer_desc desc{};
        desc.size = std::bit_ceil(instances_.size()) * sizeof(Instance);
        desc.usage.vertex_buffer = true;
        desc.usage.write_transient = true;
        desc.label = "particle-instance-stream";
        const auto buffer = sg_make_buffer(desc);
        try {
            RequireGpuResource(sg_query_buffer_state(buffer), buffer.id, BufferPool, "particle instance stream");
        } catch (...) { sg_destroy_buffer(buffer); throw; }
        sg_destroy_buffer(stream.Buffer);
        stream = {buffer, desc.size};
    }
    sg_write_buffer_desc write{};
    write.src.data = {instances_.data(), bytes};
    write.dst.buffer = stream.Buffer;
    sg_write_buffer_transient(write);
}

std::size_t ParticleRenderer::Draw(const Mat4& projection) {
    std::size_t computeCalls = 0;
#if !defined(__EMSCRIPTEN__)
    if (compute_ && compute_->Draw(projection)) { computeCalls = 1; }
#endif
    if (instances_.empty()) { return computeCalls; }
    usedStreams_ = std::max(usedStreams_, preparedStream_ + 1);
    for (const auto& run : runs_) {
        const auto& material = run.Surface;
        const auto* mesh = material.Geometry == ParticleRendererType::Mesh ? meshes_.at(material.Mesh).get() : nullptr;
        sg_bindings bindings{};
        bindings.vertex_buffers[0] = mesh ? mesh->Vertices : corners_;
        bindings.vertex_buffers[1] = streams_[preparedStream_].Buffer;
        bindings.vertex_buffer_offsets[1] = static_cast<int>(run.First * sizeof(Instance));
        bindings.views[VIEW_particle_texture] = material.Texture ? textures_.at(material.Texture)->View : fallback_->View;
        bindings.samplers[SMP_particle_sampler] = sampler_;
        particle_params_t params{};
        params.projection = projection;
        params.view = view_;
        params.kind[0] = static_cast<float>(material.Geometry);
        params.atlas[0] = material.Columns;
        params.atlas[1] = material.Rows;
        if (material.Texture) {
            params.atlas[2] = 0.5f / static_cast<float>(material.Texture->Width());
            params.atlas[3] = 0.5f / static_cast<float>(material.Texture->Height());
        }
        const bool additive = material.Blend == ParticleBlend::Additive;
        sg_apply_pipeline(mesh ? (additive ? meshAdditive_ : meshAlpha_) : (additive ? additivePipeline_ : alphaPipeline_));
        sg_apply_bindings(bindings);
        sg_apply_uniforms(UB_particle_params, SG_RANGE(params));
        sg_draw(0, mesh ? mesh->Count : 4, static_cast<int>(run.Count));
    }
    return runs_.size() + computeCalls;
}

std::size_t ParticleRenderer::BufferBytes() const noexcept {
    std::size_t bytes = corners_.id ? 12 * sizeof(float) : 0;
    for (const auto& [key, mesh] : meshes_) { (void)key; bytes += static_cast<std::size_t>(mesh->Count) * sizeof(Vec3); }
    for (const auto& stream : streams_) { bytes += stream.Bytes; }
#if !defined(__EMSCRIPTEN__)
    if (compute_) { bytes += compute_->Bytes(); }
#endif
    return bytes;
}

std::size_t ParticleRenderer::TextureBytes() const noexcept {
    std::size_t bytes = fallback_ ? 4 : 0;
    for (const auto& [texture, gpu] : textures_) { (void)gpu; bytes += texture->Pixels().size(); }
    return bytes;
}

void ParticleRenderer::Inspect(std::vector<DebugResource>& resources) const {
    InspectShader(resources, shader_, "Particle sprites");
    InspectPipeline(resources, additivePipeline_, "Particle additive");
    InspectPipeline(resources, alphaPipeline_, "Particle alpha");
    InspectBuffer(resources, corners_, "Particle quad");
    for (const auto& [model, mesh] : meshes_) { (void)model; InspectBuffer(resources, mesh->Vertices, "Particle mesh"); }
    for (const auto& [texture, gpu] : textures_) { (void)texture; InspectImage(resources, gpu->Image, "Particle flipbook"); }
    for (auto stream : streams_) { InspectBuffer(resources, stream.Buffer, "Particle instance stream"); }
#if !defined(__EMSCRIPTEN__)
    if (compute_) { compute_->Inspect(resources); }
#endif
}
} // namespace lv::detail
