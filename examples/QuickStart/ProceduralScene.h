#pragma once

#include <minvex/Scene.h>

#include <algorithm>
#include <cmath>
#include <memory>

// Shared by the native and browser entry points.
class ProceduralScene {
public:
    void Initialize(lv::Scene& scene) {
        auto cube = std::make_shared<lv::ModelAsset>();
        cube->Meshes.push_back({"Cube", {{lv::MakeCube(), 0, false}}});
        cube->Parts.push_back({0});
        cube->Materials.push_back({});
        cube->ModelBounds = {{-.5f, -.5f, -.5f}, {.5f, .5f, .5f}};

        const auto base = scene.CreateObject("Base");
        AddBox(scene, cube, base, {0, .15f, 0}, {1, .3f, 1}, {.38f, .48f, .64f, 1});
        AddBox(scene, cube, base, {0, .52f, 0}, {.32f, .5f, .32f}, {.38f, .48f, .64f, 1});
        shoulder_ = scene.CreateObject("Shoulder", base);
        ApplyAngle(scene);
        AddBox(scene, cube, shoulder_, {.55f, 0, 0}, {1.1f, .22f, .28f}, {.24f, .64f, .95f, 1});
        AddBox(scene, cube, shoulder_, {1.15f, -.1f, 0}, {.26f, .45f, .35f}, {1, .64f, .26f, 1});

        scene.MainCamera.Position = {3, 2.4f, 4.2f};
        scene.MainCamera.Target = {0, .55f, 0};
    }

    void Turn(float direction) { target_ = std::clamp(target_ + direction * .45f, -1.35f, 1.35f); }

    void Update(lv::Scene& scene, double deltaSeconds) {
        const auto step = static_cast<float>(deltaSeconds * 2.5);
        const auto next = angle_ + std::clamp(target_ - angle_, -step, step);
        if (next == angle_) { return; }
        angle_ = next;
        ApplyAngle(scene);
    }

    float Angle() const { return angle_; }

private:
    static void AddBox(lv::Scene& scene, const lv::ModelHandle& cube, lv::ObjectId parent,
                       lv::Vec3 position, lv::Vec3 scale, std::array<float, 4> color) {
        lv::Transform transform;
        transform.Position = position;
        transform.Scale = scale;
        const auto box = scene.AddModel(cube, transform, parent);
        auto material = scene.GetMaterial(box, 0);
        material.BaseColor = color;
        scene.SetMaterial(box, 0, material);
    }

    void ApplyAngle(lv::Scene& scene) const {
        lv::Transform transform;
        transform.Position = {0, .75f, 0};
        transform.Rotation = {0, std::sin(angle_ * .5f), 0, std::cos(angle_ * .5f)};
        scene.SetTransform(shoulder_, transform);
    }

    lv::ObjectId shoulder_ = lv::NoObject;
    float angle_ = 0;
    float target_ = 0;
};
