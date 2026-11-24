#pragma once

#include <minvex/Debug.h>
#include <sokol_gfx.h>

#include <span>

namespace lv::detail {
class ParticleComputeRenderer final {
public:
    explicit ParticleComputeRenderer(const RenderSettings& settings);
    ~ParticleComputeRenderer();
    ParticleComputeRenderer(const ParticleComputeRenderer&) = delete;
    ParticleComputeRenderer& operator=(const ParticleComputeRenderer&) = delete;

    void Prepare(std::span<const ParticleGpuSprite> sprites, const Mat4& view);
    bool Draw(const Mat4& projection) const;
    std::size_t Count() const noexcept { return count_; }
    std::size_t Bytes() const noexcept;
    void Inspect(std::vector<DebugResource>& resources) const;

private:
    struct Stream {
        sg_buffer Staging{}, Input{}, Output{};
        sg_view InputView{}, OutputView{};
        std::size_t Capacity = 0;
        explicit Stream(std::size_t capacity);
        ~Stream();
        void Destroy() noexcept;
    };
    struct Input {
        std::array<float, 4> OriginAge, VelocityDrag, AccelerationSize;
        std::array<std::uint32_t, 4> Style;
    };
    static_assert(sizeof(Input) == 64);
    void Destroy() noexcept;
    static void OnCommit(void* user) noexcept;
    std::vector<std::unique_ptr<Stream>> streams_;
    std::vector<Input> scratch_;
    sg_shader computeShader_{}, drawShader_{};
    sg_pipeline computePipeline_{}, drawPipeline_{};
    std::size_t used_ = 0, current_ = 0, count_ = 0;
    bool listening_ = false;
};
} // namespace lv::detail
