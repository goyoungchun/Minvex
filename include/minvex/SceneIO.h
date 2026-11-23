#pragma once

#include <minvex/Result.h>
#include <minvex/Scene.h>

#include <span>

namespace lv {

// Writes version 13, or version 14 when embedding terrain; reads versions 1..14.
// External model references, embedded terrain, library, hierarchy, TRS, camera, lights, animation/pose.
// Runtime ObjectId/AssetId values are remapped when opening an archive.
struct SceneArchive {
    Scene World;
    std::vector<ModelHandle> Assets; // Includes external and embedded assets without instances.
};

// CPU-only, synchronous, caller-thread operations. 16 MiB JSON, 65536 objects,
// 4096 assets and 256 hierarchy levels. File-backed assets remain external.
// Save replaces the destination only after a complete temporary file is closed.
Result<void> SaveScene(const Scene& scene, const std::filesystem::path& path,
                       std::span<const ModelHandle> library = {});
// Failure returns no partial Scene; the supplied registry may cache loaded assets.
Result<SceneArchive> LoadScene(const std::filesystem::path& path, AssetRegistry& registry);

} // namespace lv
