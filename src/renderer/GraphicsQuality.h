#pragma once
#include <minvex/Quality.h>
#include <minvex/Scene.h>

namespace lv::detail {
QualityCapabilities QueryQualityCapabilities(); // Current D3D11/Metal context, rendering thread only.
std::uint64_t SceneGeometrySignature(std::span<const RenderItem> items);
std::uint64_t SceneRenderSignature(const Scene& scene, std::uint64_t geometry);
bool SceneHasActiveAnimation(const Scene& scene, std::span<const RenderItem> items);
}
