#pragma once
#include <sokol_gfx.h>

#include <cstdio>
#include <stdexcept>
#include <string>

namespace lv::detail {
struct GpuPool {
    const char* Name;
    int sg_desc::* Capacity;
};
inline constexpr GpuPool BufferPool{"buffer", &sg_desc::buffer_pool_size};
inline constexpr GpuPool ImagePool{"image", &sg_desc::image_pool_size};
inline constexpr GpuPool SamplerPool{"sampler", &sg_desc::sampler_pool_size};
inline constexpr GpuPool ShaderPool{"shader", &sg_desc::shader_pool_size};
inline constexpr GpuPool PipelinePool{"pipeline", &sg_desc::pipeline_pool_size};
inline constexpr GpuPool ViewPool{"view", &sg_desc::view_pool_size};
inline constexpr char GpuPoolHint[] = "Share model handles, use per-object material overrides, or reduce unique graphics resources.";

// An exhausted Sokol pool has no free slots. Report slots, which are independent of GPU byte usage.
inline void DescribeFullPool(char* output, std::size_t size, GpuPool pool) noexcept {
    const int capacity = sg_query_desc().*pool.Capacity;
    std::snprintf(output, size, "GPU %s pool exhausted (%d/%d slots occupied).", pool.Name, capacity, capacity);
}

inline bool DescribeFullPool(char* output, std::size_t size, unsigned item) noexcept {
    switch (item) {
    case SG_LOGITEM_BUFFER_POOL_EXHAUSTED: DescribeFullPool(output, size, BufferPool); return true;
    case SG_LOGITEM_IMAGE_POOL_EXHAUSTED: DescribeFullPool(output, size, ImagePool); return true;
    case SG_LOGITEM_SAMPLER_POOL_EXHAUSTED: DescribeFullPool(output, size, SamplerPool); return true;
    case SG_LOGITEM_SHADER_POOL_EXHAUSTED: DescribeFullPool(output, size, ShaderPool); return true;
    case SG_LOGITEM_PIPELINE_POOL_EXHAUSTED: DescribeFullPool(output, size, PipelinePool); return true;
    case SG_LOGITEM_VIEW_POOL_EXHAUSTED: DescribeFullPool(output, size, ViewPool); return true;
    default: return false;
    }
}

class GpuPoolExhausted : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void RequireGpuResource(sg_resource_state state, unsigned id, GpuPool pool, const char* name) {
    if (state == SG_RESOURCESTATE_VALID) { return; }
    // sg_make_* returns an invalid ID on pool exhaustion. Backend creation failures
    // retain an allocated ID in the FAILED state.
    if (!id) {
        char message[128];
        DescribeFullPool(message, sizeof(message), pool);
        throw GpuPoolExhausted(std::string(name) + ": " + message);
    }
    throw std::runtime_error(std::string("Cannot create ") + name + ".");
}
} // namespace lv::detail
