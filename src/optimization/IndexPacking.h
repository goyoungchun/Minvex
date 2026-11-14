#pragma once
#include <minvex/Mesh.h>
#include <span>
#include <stdexcept>
namespace lv::detail {
// WebGL2 always reserves 0xffff for primitive restart, even for triangle lists.
inline std::size_t IndexElementBytes(std::size_t vertexCount,bool fixedRestart=
#if defined(__EMSCRIPTEN__)
    true
#else
    false
#endif
) {return vertexCount<=(fixedRestart ? 65535u:65536u) ? sizeof(std::uint16_t):sizeof(std::uint32_t);}
inline std::vector<std::uint16_t> PackIndices16(std::span<const std::uint32_t> indices) {
    std::vector<std::uint16_t> result;result.reserve(indices.size());
    for (auto index:indices) {if (index>65535) {throw std::invalid_argument("16-bit index overflow.");}result.push_back(static_cast<std::uint16_t>(index));}return result;
}
}
