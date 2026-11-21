#pragma once

#include <minvex/Scene.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace maintenance {

inline constexpr std::array<std::string_view, 8> PartIds{
    "housing", "cover", "drive_gear", "driven_gear", "bolt_fl", "bolt_fr", "bolt_bl", "bolt_br"
};

struct Part {
    lv::ModelHandle Model;
    lv::Transform Assembled;
};
using Assembly = std::array<Part, PartIds.size()>;

// Scene edits act on whole objects. Give each distinct mesh an immutable model,
// and let named nodes that use the same mesh share that model.
inline lv::ModelHandle MakePartModel(const lv::ModelAsset& source, std::size_t meshIndex) {
    auto model = std::make_shared<lv::ModelAsset>();
    model->Format = lv::AssetFormat::Custom;
    model->Meshes.push_back(source.Meshes.at(meshIndex));
    model->Parts.push_back({0});

    std::unordered_map<std::size_t, std::size_t> materials, textures, images;
    const auto copyTexture = [&](std::optional<std::size_t>& index) {
        if (!index) { return; }
        const auto [texture, added] = textures.try_emplace(*index, model->Textures.size());
        if (added) {
            auto value = source.Textures.at(*index);
            const auto [image, newImage] = images.try_emplace(value.ImageIndex, model->Images.size());
            if (newImage) { model->Images.push_back(source.Images.at(value.ImageIndex)); }
            value.ImageIndex = image->second;
            model->Textures.push_back(value);
        }
        index = texture->second;
    };

    bool firstVertex = true;
    for (auto& primitive : model->Meshes.front().Primitives) {
        if (primitive.HasSkinning || !primitive.MorphTargets.empty()) {
            throw std::invalid_argument("Maintenance parts must use rigid geometry.");
        }
        const auto [material, added] = materials.try_emplace(primitive.MaterialIndex, model->Materials.size());
        if (added) {
            auto value = source.Materials.at(primitive.MaterialIndex);
            copyTexture(value.BaseColorTexture);
            copyTexture(value.NormalTexture);
            copyTexture(value.MetallicRoughnessTexture);
            model->Materials.push_back(std::move(value));
        }
        primitive.MaterialIndex = material->second;

        for (const auto& vertex : primitive.Geometry.Vertices) {
            const auto& p = vertex.Position;
            const lv::Vec3 position{p[0], p[1], p[2]};
            auto& bounds = model->ModelBounds;
            if (firstVertex) {
                bounds = {position, position};
                firstVertex = false;
            }
            bounds.Min = {std::min(bounds.Min.X, p[0]), std::min(bounds.Min.Y, p[1]), std::min(bounds.Min.Z, p[2])};
            bounds.Max = {std::max(bounds.Max.X, p[0]), std::max(bounds.Max.Y, p[1]), std::max(bounds.Max.Z, p[2])};
        }
    }
    lv::ValidateModel(*model);
    return model;
}

inline Assembly LoadAssembly(const std::filesystem::path& path) {
    auto extension = path.extension().u8string();
    for (auto& c : extension) {
        if (c >= u8'A' && c <= u8'Z') { c += u8'a' - u8'A'; }
    }
    if (extension != u8".glb") { throw std::invalid_argument("Select a .glb assembly."); }
    const auto source = lv::ImporterRegistry{}.Import(path);
    if (!source->Animations.empty() || !source->Skins.empty()) {
        throw std::invalid_argument("Export a rigid GLB without animation or skinning.");
    }

    Assembly assembly{};
    std::unordered_map<std::size_t, lv::ModelHandle> meshes;
    for (const auto& part : source->Parts) {
        const auto& name = source->Nodes.at(part.Node).Name;
        const auto id = std::find(PartIds.begin(), PartIds.end(), name);
        if (id == PartIds.end()) { throw std::invalid_argument("Unexpected renderable node: " + name); }
        auto& slot = assembly[static_cast<std::size_t>(id - PartIds.begin())];
        if (slot.Model) { throw std::invalid_argument("Duplicate part ID: " + name); }
        // ModelPart already includes the GLB node's parent transforms.
        slot.Assembled = lv::DecomposeTRS(part.Transform);
        const auto [mesh, added] = meshes.try_emplace(part.MeshIndex);
        if (added) { mesh->second = MakePartModel(*source, part.MeshIndex); }
        slot.Model = mesh->second;
    }
    for (std::size_t i = 0; i < assembly.size(); ++i) {
        if (!assembly[i].Model) {
            throw std::invalid_argument("Missing part ID: " + std::string(PartIds[i]));
        }
    }
    return assembly;
}

} // namespace maintenance
