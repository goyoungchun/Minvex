#include "ParticleComputeRenderer.h"
#include "DebugResources.h"
#include "particle_compute.glsl.h"
#include "platform/GpuResourceError.h"

#include <bit>

namespace lv::detail {
namespace {
constexpr std::size_t OutputBytes = 32;
sg_buffer ResourceBuffer(std::size_t bytes, sg_buffer_usage usage, const char* label) {
    sg_buffer_desc desc{};
    desc.size = bytes;
    desc.usage = usage;
    desc.label = label;
    const auto resource = sg_make_buffer(desc);
    try { RequireGpuResource(sg_query_buffer_state(resource), resource.id, BufferPool, label); }
    catch (...) { sg_destroy_buffer(resource); throw; }
    return resource;
}
sg_view StorageView(sg_buffer buffer) {
    sg_view_desc desc{};
    desc.storage_buffer.buffer = buffer;
    const auto view = sg_make_view(desc);
    try { RequireGpuResource(sg_query_view_state(view), view.id, ViewPool, "particle compute storage view"); }
    catch (...) { sg_destroy_view(view); throw; }
    return view;
}
}

ParticleComputeRenderer::Stream::Stream(std::size_t capacity) : Capacity(capacity) {
    try {
        sg_buffer_usage usage{};
        usage.staging_buffer = usage.write_transient = usage.copy_src = true;
        Staging = ResourceBuffer(capacity * sizeof(ParticleComputeRenderer::Input), usage, "particle-compute-upload");
        usage = {};
        usage.storage_buffer = usage.copy_dst = true;
        Input = ResourceBuffer(capacity * sizeof(ParticleComputeRenderer::Input), usage, "particle-compute-input");
        InputView = StorageView(Input);
        usage.copy_dst = false;
        Output = ResourceBuffer(capacity * OutputBytes, usage, "particle-compute-output");
        OutputView = StorageView(Output);
    } catch (...) { Destroy(); throw; }
}

ParticleComputeRenderer::Stream::~Stream() { Destroy(); }
void ParticleComputeRenderer::Stream::Destroy() noexcept {
    sg_destroy_view(OutputView);
    sg_destroy_view(InputView);
    sg_destroy_buffer(Output);
    sg_destroy_buffer(Input);
    sg_destroy_buffer(Staging);
}

ParticleComputeRenderer::ParticleComputeRenderer(const RenderSettings& settings) {
    try {
        const auto backend = sg_query_backend();
        computeShader_ = sg_make_shader(particle_compute_shader_desc(backend));
        RequireGpuResource(sg_query_shader_state(computeShader_), computeShader_.id, ShaderPool, "particle compute shader");
        sg_pipeline_desc pipeline{};
        pipeline.shader = computeShader_;
        pipeline.compute = true;
        pipeline.label = "particle-compute-pipeline";
        computePipeline_ = sg_make_pipeline(pipeline);
        RequireGpuResource(sg_query_pipeline_state(computePipeline_), computePipeline_.id, PipelinePool, "particle compute pipeline");
        drawShader_ = sg_make_shader(particle_compute_draw_shader_desc(backend));
        RequireGpuResource(sg_query_shader_state(drawShader_), drawShader_.id, ShaderPool, "particle compute draw shader");
        pipeline = {};
        pipeline.shader = drawShader_;
        pipeline.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
        pipeline.sample_count = settings.MsaaSamples;
        pipeline.colors[0].pixel_format = settings.PostProcessing ? SG_PIXELFORMAT_RGBA16F :
            sg_query_desc().environment.defaults.color_format;
        pipeline.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        auto& blend = pipeline.colors[0].blend;
        blend.enabled = true;
        blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        blend.src_factor_alpha = SG_BLENDFACTOR_ZERO;
        blend.dst_factor_alpha = SG_BLENDFACTOR_ONE;
        pipeline.label = "particle-compute-draw";
        drawPipeline_ = sg_make_pipeline(pipeline);
        RequireGpuResource(sg_query_pipeline_state(drawPipeline_), drawPipeline_.id, PipelinePool, "particle compute draw pipeline");
        listening_ = sg_add_commit_listener({OnCommit, this});
        if (!listening_) { throw std::runtime_error("Cannot register the particle compute commit listener."); }
    } catch (...) { Destroy(); throw; }
}

ParticleComputeRenderer::~ParticleComputeRenderer() { Destroy(); }
void ParticleComputeRenderer::Destroy() noexcept {
    if (listening_) { sg_remove_commit_listener({OnCommit, this}); }
    streams_.clear();
    sg_destroy_pipeline(drawPipeline_);
    sg_destroy_pipeline(computePipeline_);
    sg_destroy_shader(drawShader_);
    sg_destroy_shader(computeShader_);
}

void ParticleComputeRenderer::OnCommit(void* user) noexcept {
    auto& self = *static_cast<ParticleComputeRenderer*>(user);
    self.used_ = self.count_ = 0;
}

void ParticleComputeRenderer::Prepare(std::span<const ParticleGpuSprite> sprites, const Mat4& view) {
    count_ = 0;
    if (sprites.empty()) { return; }
    scratch_.clear();
    if (scratch_.capacity() < sprites.size()) { scratch_.reserve(std::bit_ceil(sprites.size())); }
    for (const auto& sprite : sprites) {
        const auto color = sprite.Appearance.Color;
        std::uint32_t packed = 0;
        for (unsigned i = 0; i < 4; ++i) { packed |= static_cast<std::uint32_t>(color[i] * 255 + 0.5f) << (8 * i); }
        scratch_.push_back({{sprite.Origin.X, sprite.Origin.Y, sprite.Origin.Z, sprite.Age},
            {sprite.Velocity.X, sprite.Velocity.Y, sprite.Velocity.Z, sprite.Drag},
            {sprite.Acceleration.X, sprite.Acceleration.Y, sprite.Acceleration.Z, sprite.Appearance.Size},
            {std::bit_cast<std::uint32_t>(sprite.Appearance.Rotation), std::bit_cast<std::uint32_t>(sprite.Appearance.AspectRatio),
             std::bit_cast<std::uint32_t>(sprite.Appearance.MaxDrawDistance), packed}});
    }
    current_ = used_;
    if (current_ == streams_.size()) { streams_.emplace_back(); }
    auto& slot = streams_[current_];
    if (!slot || slot->Capacity < sprites.size()) { slot = std::make_unique<Stream>(std::bit_ceil(sprites.size())); }
    const auto bytes = scratch_.size() * sizeof(Input);
    sg_write_buffer_desc write{};
    write.src.data = {scratch_.data(), bytes};
    write.dst.buffer = slot->Staging;
    sg_write_buffer_transient(write);
    sg_copy_buffer_to_buffer_desc copy{};
    copy.src.buffer = slot->Staging;
    copy.dst.buffer = slot->Input;
    copy.size = bytes;
    sg_copy_buffer_to_buffer(copy);
    sg_pass pass{};
    pass.compute = true;
    pass.label = "particle simulation";
    sg_begin_pass(pass);
    sg_apply_pipeline(computePipeline_);
    sg_bindings bindings{};
    bindings.views[VIEW_particle_input] = slot->InputView;
    bindings.views[VIEW_particle_output] = slot->OutputView;
    sg_apply_bindings(bindings);
    particle_compute_params_t params{};
    params.view = view;
    params.counts[0] = static_cast<int>(sprites.size());
    sg_apply_uniforms(UB_particle_compute_params, SG_RANGE(params));
    sg_dispatch((params.counts[0] + 63) / 64, 1, 1);
    sg_end_pass();
    ++used_;
    count_ = sprites.size();
}

bool ParticleComputeRenderer::Draw(const Mat4& projection) const {
    if (!count_) { return false; }
    sg_apply_pipeline(drawPipeline_);
    sg_bindings bindings{};
    bindings.views[VIEW_particle_draw_buffer] = streams_[current_]->OutputView;
    sg_apply_bindings(bindings);
    const particle_compute_draw_params_t params{projection};
    sg_apply_uniforms(UB_particle_compute_draw_params, SG_RANGE(params));
    sg_draw(0, 4, static_cast<int>(count_));
    return true;
}

std::size_t ParticleComputeRenderer::Bytes() const noexcept {
    std::size_t bytes = 0;
    for (const auto& stream : streams_) { if (stream) { bytes += stream->Capacity * (2 * sizeof(Input) + OutputBytes); } }
    return bytes;
}

void ParticleComputeRenderer::Inspect(std::vector<DebugResource>& resources) const {
    InspectPipeline(resources, computePipeline_, "Particle GPU simulation");
    InspectPipeline(resources, drawPipeline_, "Particle GPU sprites");
    for (const auto& stream : streams_) {
        if (stream) { InspectBuffer(resources, stream->Output, "Particle GPU simulation output"); }
    }
}
} // namespace lv::detail
