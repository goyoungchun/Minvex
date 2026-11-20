#pragma once

#include <minvex/Scene.h>

#include <cmath>
#include <memory>
#include <numbers>
#include <utility>

namespace skinning {
inline lv::ObjectId AddArm(lv::Scene& scene) {
    constexpr float elbowX = 1;
    auto model = std::make_shared<lv::ModelAsset>();
    model->Format = lv::AssetFormat::Custom;

    // Skeleton entries use the same indices and parents as ModelNode entries.
    // The mesh node is not a bone. It keeps vertices in model space at bind time.
    constexpr lv::JointId meshNode = 0, root = 1, elbow = 2;
    model->Nodes.resize(3);
    model->Nodes[meshNode].Name = "Arm mesh";
    model->Nodes[meshNode].MeshIndex = 0;
    model->Nodes[meshNode].SkinIndex = 0;
    model->Nodes[root].Name = "Upper arm";
    model->Nodes[elbow].Name = "Elbow";
    model->Nodes[elbow].Parent = root;

    lv::Transform elbowLocal;
    elbowLocal.Position = {elbowX, 0, 0};
    const auto elbowBind = elbowLocal.Matrix(); // Root bind transform is identity.
    model->Nodes[elbow].LocalTransform = elbowBind;
    model->Skeleton.Joints = {
        {"Arm mesh"},
        {"Upper arm", lv::NoJoint, {}, lv::Mat4::Identity(), true, true},
        {"Elbow", root, elbowLocal, elbowBind, true, true}
    };
    model->Skeleton.Signature = lv::SkeletonSignature(model->Skeleton);

    // Vertex joints are palette slots 0 and 1. Those map to skeleton nodes 1 and 2.
    // Each inverse bind converts a model-space bind vertex to the bone's local space.
    model->Skins.push_back({"Arm skin", {root, elbow}, {lv::Mat4::Identity(), elbowBind.InverseAffine()}});

    lv::MeshData ribbon;
    constexpr float halfWidth = .16f;
    constexpr std::array<float, 5> positions{0, .75f, elbowX, 1.25f, 2};
    constexpr std::array<float, 5> elbowWeights{0, 0, .5f, 1, 1};
    for (std::uint32_t row = 0; row < positions.size(); ++row) {
        const float weight = elbowWeights[row];
        for (const float y : {-halfWidth, halfWidth}) {
            lv::Vertex vertex{};
            vertex.Position = {positions[row], y, 0};
            vertex.Normal = {0, 0, 1};
            vertex.Color = {1, .5f + .35f * weight, .2f, 1};
            vertex.Joints = {0, 1, 0, 0};
            vertex.Weights = {1 - weight, weight, 0, 0};
            ribbon.Vertices.push_back(vertex);
        }
        if (row > 0) {
            const auto previous = 2 * (row - 1);
            ribbon.Indices.insert(ribbon.Indices.end(),
                {previous, previous + 2, previous + 1, previous + 1, previous + 2, previous + 3});
        }
    }
    model->Meshes.push_back({"Arm ribbon", {{std::move(ribbon), 0, true}}});
    model->Parts.push_back({0, lv::Mat4::Identity(), meshNode, 0});
    lv::MaterialAsset material;
    material.Shading = lv::ShadingModel::Unlit;
    model->Materials.push_back(material);
    model->ModelBounds = {{0, -halfWidth, 0}, {2, halfWidth, 0}};

    const float halfBend = std::numbers::pi_v<float> / 4; // 90 degrees around +Z.
    lv::AnimationTrack bend;
    bend.Joint = elbow; // Tracks use skeleton node IDs, not palette slots.
    bend.Path = lv::AnimationPath::Rotation;
    bend.Times = {0, 1, 2};
    bend.Values = {{0, 0, 0, 1}, {0, 0, std::sin(halfBend), std::cos(halfBend)}, {0, 0, 0, 1}};
    lv::AnimationClip clip;
    clip.Name = "Bend";
    clip.Duration = 2;
    clip.Tracks.push_back(std::move(bend));
    model->Animations.push_back(std::move(clip));

    lv::ValidateModel(*model); // Validate geometry, hierarchy, palette and clip before publishing.
    const auto arm = scene.AddModel(std::move(model));
    scene.MainCamera.Position = {1, .4f, 4};
    scene.MainCamera.Target = {1, .4f, 0};
    scene.MainCamera.Projection = lv::CameraProjection::Orthographic;
    scene.MainCamera.OrthographicHeight = 2.2f;
    return arm;
}
} // namespace skinning
