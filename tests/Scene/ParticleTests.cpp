#include <minvex/Scene.h>
#include <minvex/Terrain.h>
#include "TestHarness.h"

#include <algorithm>
#include <limits>

using namespace test;

namespace {
void TimingAndCurves() {
    lv::ParticleEmitter emitter;
    emitter.Spawn.Rate = 4;
    emitter.Spawn.Duration = 1;
    emitter.Spawn.Bursts = {{0.5, 2}, {0, 1}};
    emitter.Initial.Lifetime = {2, 2};
    emitter.Initial.Size = {1, 1};
    emitter.Initial.Velocity = {{0, 2, 0}, {0, 2, 0}};
    emitter.Initial.AngularVelocity = {2, 2};
    emitter.Acceleration = {0, -2, 0};
    emitter.ColorOverLife = {{0, {1, 0, 0, 1}}, {1, {0, 0, 1, 0}}};
    emitter.SizeOverLife = {{0, 1}, {1, 3}};
    const auto effect = lv::MakeParticleEffect({emitter});
    lv::Scene scene;
    const auto id = scene.CreateObject();
    scene.SetParticleEffect(id, effect, 71);
    scene.UpdateParticles(0);
    scene.UpdateParticles(0);
    Check(scene.ParticleCount(id) == 1, "time-zero burst fires once");
    scene.UpdateParticles(0.5);
    std::vector<lv::ParticleSprite> sprites;
    scene.CollectParticleSprites(sprites);
    Check(sprites.size() == 5, "timed burst and rate requests merge at the same boundary");
    if (sprites.size() == 5) {
        const auto& first = sprites.front();
        Check(Near(first.Position.Y, 0.75f) && Near(first.Rotation, 1) && Near(first.Size, 1.5f) &&
              Near(first.Color[0], 0.75f) && Near(first.Color[2], 0.25f) && Near(first.Color[3], 0.75f),
              "closed-form motion and lifetime curves agree with elapsed seconds");
        Check(Near(sprites[1].Position.Y, 0.4375f) && Near(sprites.back().Position.Y, 0),
              "rate births use their time inside the step");
    }
    scene.SetParticleEmission(id, false);
    scene.UpdateParticles(2);
    Check(scene.ParticleCount() == 0, "disabled emission drains live particles");
    scene.SetParticleEmission(id, true);
    scene.UpdateParticles(0.5);
    Check(scene.ParticleCount() == 2, "resume uses the remaining duration and rate phase");
    scene.UpdateParticles(2);
    Check(scene.ParticleCount() == 0, "one-shot duration prevents later rate emission");
    scene.RestartParticles(id);
    scene.UpdateParticles(0);
    Check(scene.ParticleCount() == 1, "restart re-arms the burst");
}

void SpacesAndLifetime() {
    lv::ParticleEmitter emitter;
    emitter.Spawn = {0, 0, {{0, 1}}};
    emitter.Initial.Position = {{1, 0, 0}, {1, 0, 0}};
    auto local = emitter;
    local.Space = lv::ParticleSpace::Local;
    lv::Scene scene;
    const auto parent = scene.CreateObject();
    const auto id = scene.CreateObject("Effect", parent);
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter, local}));
    lv::Transform transform;
    transform.Position.X = 2;
    scene.SetTransform(parent, transform);
    scene.UpdateParticles(0);
    transform.Position.X = 4;
    scene.SetTransform(parent, transform);
    std::vector<lv::ParticleSprite> sprites;
    scene.CollectParticleSprites(sprites);
    Check(sprites.size() == 2 && Near(sprites[0].Position.X, 3) && Near(sprites[1].Position.X, 5),
          "world particles retain their birth transform while local particles follow the hierarchy");
    scene.SetVisible(parent, false);
    scene.CollectParticleSprites(sprites);
    Check(sprites.empty() && scene.ParticleCount() == 2, "parent visibility hides rendering and preserves simulation");
    lv::Scene moved = std::move(scene);
    moved.RemoveObject(parent);
    Check(!moved.HasParticleEffects() && moved.ParticleCount() == 0, "removing a parent releases child effect storage");
}

void DeterminismAndBudget() {
    lv::ParticleEmitter emitter;
    emitter.Capacity = 5;
    emitter.Spawn = {20, 0, {{0.25, 9}, {0.375, 2}}};
    emitter.Initial.Lifetime = {0.1f, 1.25f};
    emitter.Initial.Position = {{-1, -2, -3}, {1, 2, 3}};
    emitter.Initial.Velocity = {{0, 1, 0}, {0, 2, 0}};
    const auto effect = lv::MakeParticleEffect({emitter});
    lv::Scene scene;
    const auto id = scene.CreateObject();
    scene.SetParticleEffect(id, effect, 9);
    scene.UpdateParticles(0.5);
    std::vector<lv::ParticleSprite> first, split;
    scene.CollectParticleSprites(first);
    scene.RestartParticles(id);
    for (int i = 0; i < 8; ++i) { scene.UpdateParticles(0.0625); }
    scene.CollectParticleSprites(split);
    Check(!first.empty() && first.size() <= emitter.Capacity && first.size() == split.size() &&
          std::equal(first.begin(), first.end(), split.begin(), [](const auto& a, const auto& b) {
              return Near(a.Position.X, b.Position.X) && Near(a.Position.Y, b.Position.Y) && Near(a.Position.Z, b.Position.Z);
          }), "seeded spawns replay across different timestep partitions even when capacity is saturated");

    emitter.Capacity = 3;
    emitter.Spawn = {100000, 0, {{0, std::numeric_limits<std::uint32_t>::max()}}};
    emitter.Initial.Lifetime = {1, 1};
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter}));
    scene.UpdateParticles(lv::MaxParticleStepSeconds);
    Check(scene.ParticleCount() == 3, "an oversized burst and a day of continuous requests respect the cap");
    Rejects([&] { scene.UpdateParticles(-1); }, "negative particle delta is rejected");
    Rejects([&] { scene.UpdateParticles(std::numeric_limits<double>::quiet_NaN()); }, "nonfinite particle delta is rejected");
    Check(scene.ParticleCount() == 3, "invalid updates preserve state");

    emitter.Spawn = {0, 0, {}};
    emitter.Capacity = lv::MaxParticlesPerEmitter;
    const auto large = lv::MakeParticleEffect({emitter, emitter});
    const auto second = scene.CreateObject();
    try { scene.SetParticleEffect(second, large); Check(false, "scene capacity constraint"); }
    catch (const std::length_error&) { Check(scene.ParticleCount(id) == 3, "over-budget attachment preserves prior state"); }
    scene.SetParticleEffect(id, {});
    scene.SetParticleEffect(second, large);
    Check(scene.HasParticleEffects() && scene.ParticleCount() == 0, "detaching releases budget for reuse");
    emitter.SizeOverLife = {{0.5f, 1}, {0.5f, 2}};
    Rejects([&] { (void)lv::MakeParticleEffect({emitter}); }, "duplicate curve times are rejected");
}

void ExtendedModules() {
    lv::ParticleEmitter emitter;
    emitter.Spawn = {0, 0.5, {{0, 1}, {0.25, 2}}, 0.25, 3};
    emitter.Initial.Lifetime = {2, 2};
    emitter.Shape = lv::ParticleShape::Cone;
    emitter.Radius = 0;
    emitter.ConeAngle = 0;
    emitter.RadialSpeed = 2;
    emitter.Drag = 1;
    emitter.Flipbook = {lv::LoadParticleTexture(Fixture("ParticleFlipbook.png")), 2, 2, 1};
    auto disabled = emitter;
    disabled.Enabled = false;
    lv::Scene scene;
    const auto id = scene.CreateObject();
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter, disabled}), 77);
    Check(scene.GetParticleEffect(id).Seed == 77, "binding retains the authored seed");
    scene.UpdateParticles(0.125);
    Check(scene.ParticleCount(id) == 0, "delay gates the initial burst");
    scene.UpdateParticles(0.125);
    Check(scene.ParticleCount(id) == 1, "zero-time burst fires as soon as the delay ends");
    scene.SeekParticles(id, 1.25);
    std::vector<lv::ParticleSprite> sprites;
    scene.CollectParticleSprites(sprites);
    Check(scene.ParticleTime(id) == 1.25 && sprites.size() == 7, "seek replays exactly three partially elapsed cycles");
    if (sprites.size() == 7) {
        Check(Near(sprites[0].Position.Y, 2 * (1 - std::exp(-1.0f))) && Near(sprites[0].Position.X, 0) &&
              sprites[0].Texture && sprites[0].Frame == 2 && sprites[6].Frame == 0,
              "cone velocity, closed-form drag and lifetime flipbook select the expected state");
    }
    scene.UpdateParticles(0.5);
    Check(scene.ParticleCount(id) == 9, "finite cycles stop spawning at their duration");
    emitter.Spawn.Cycles = 0;
    emitter.Spawn.Duration = 0.005;
    emitter.Spawn.Bursts = {{0, std::numeric_limits<std::uint32_t>::max()}};
    emitter.Capacity = 5;
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter}));
    scene.SeekParticles(id, lv::MaxParticleStepSeconds);
    Check(scene.ParticleCount(id) == 5, "repeated oversized bursts remain bounded on a full-day seek");
}

void CollisionAndTrails() {
    lv::ParticleEmitter emitter;
    emitter.Spawn = {0, 0, {{0, 1}}};
    emitter.Initial.Lifetime = {3, 3};
    emitter.Initial.Position = {{0, 1, 0}, {0, 1, 0}};
    emitter.Acceleration = {0, -2, 0};
    emitter.Collision = {lv::ParticleCollision::Bounce, 0, 0.5f};
    emitter.Trail = {0.5f, 2, 1};
    lv::Scene scene;
    const auto id = scene.CreateObject();
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter}));
    scene.SeekParticles(id, 1.5);
    std::vector<lv::ParticleSprite> sprites;
    scene.CollectParticleSprites(sprites);
    Check(scene.ParticleCount(id) == 1 && sprites.size() == 3 && Near(sprites[0].Position.Y, 0.25f) &&
          sprites[1].TrailEnd && Near(sprites[1].TrailEnd->Y, 0.1875f) &&
          sprites[2].TrailEnd && Near(sprites[2].TrailEnd->Y, 0), "bounce and historical trail samples agree at fixed times");
    emitter.Collision.Mode = lv::ParticleCollision::Kill;
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter}));
    scene.SeekParticles(id, 0.5);
    Check(scene.ParticleCount(id) == 1, "plane kill leaves particles alive before contact");
    scene.UpdateParticles(0.5);
    Check(scene.ParticleCount(id) == 0, "plane kill removes particles on contact");
}

void EventRoutesAndGraph() {
    lv::ParticleEmitter child, source;
    source.Spawn = {0, 0, {{0, 1}}};
    source.Initial.Lifetime = {0.5f, 0.5f};
    source.EmitEvents = true;
    source.EventRoutes = {{lv::ParticleEventKind::Death, 0, 1}};
    child.Spawn.Rate = 0;
    child.Space = lv::ParticleSpace::Local;
    child.Initial.Position = {{1, 0, 0}, {1, 0, 0}};
    child.Initial.Velocity = {{2, 0, 0}, {2, 0, 0}};
    child.Initial.Size = {1, 1};
    child.Graph.Nodes = {{lv::ParticleNodeOp::Multiply, 1, 2}, {lv::ParticleNodeOp::Age},
                         {lv::ParticleNodeOp::Constant, 255, 255, 2}, {lv::ParticleNodeOp::Constant, 255, 255, 0.5f}};
    child.Graph.Size = 0;
    child.Graph.Opacity = 3;
    child.Graph.Speed = 2;
    lv::Scene scene;
    const auto id = scene.CreateObject();
    lv::Transform placement;
    placement.Position.X = 5;
    scene.SetTransform(id, placement);
    scene.SetParticleEffect(id, lv::MakeParticleEffect({child, source}));
    scene.UpdateParticles(0.75);
    const auto events = scene.PollParticleEvents();
    Check(events.Events.size() == 2 && events.Events[0].Kind == lv::ParticleEventKind::Birth &&
          events.Events[1].Kind == lv::ParticleEventKind::Death && events.Events[1].Time == 0.5 &&
          events.Events[1].Emitter == 1 && events.Dropped == 0 && scene.PollParticleEvents().Events.empty(),
          "events retain correct timing, emitter and drain semantics across an entire short lifetime");
    std::vector<lv::ParticleSprite> sprites;
    scene.CollectParticleSprites(sprites);
    Check(sprites.size() == 1 && Near(sprites[0].Position.X, 7) && Near(sprites[0].Size, 0.5f) && Near(sprites[0].Color[3], 0.5f),
          "child uses the parent's world event position, remainder of the step, and compiled graph outputs");
    scene.SeekParticles(id, 0.75);
    Check(scene.ParticleCount(id) == 1 && scene.PollParticleEvents().Events.empty(), "seek replays routes without publishing events");
    child.EventRoutes = {{lv::ParticleEventKind::Birth, 1, 1}};
    Rejects([&] { (void)lv::MakeParticleEffect({child, source}); }, "cyclic emitter graphs are rejected");
    child.EventRoutes.clear();
    child.Graph.Nodes[1] = {lv::ParticleNodeOp::Sine, 0};
    Rejects([&] { (void)lv::MakeParticleEffect({child}); }, "cyclic module graphs are rejected");
}

void SceneSurfaceCollisions() {
    lv::Scene scene;
    scene.AddModel(lv::MakeTerrainModel(lv::TerrainGrid({2,2,2,2,{}}, std::array<double,4>{})));
    auto cube = std::make_shared<lv::ModelAsset>();
    cube->Meshes.push_back({"Cube", {{lv::MakeCube(), 0, false}}});
    cube->Parts.push_back({0});
    cube->Materials.push_back({});
    cube->ModelBounds = {{-0.5f,-0.5f,-0.5f},{0.5f,0.5f,0.5f}};
    const auto mesh = scene.AddModel(cube);
    lv::ParticleEmitter emitter;
    emitter.Spawn = {0, 0, {{0, 1}}};
    emitter.Initial.Position = {{0,1,0},{0,1,0}};
    emitter.Initial.Velocity = {{0,-2,0},{0,-2,0}};
    emitter.EmitEvents = true;
    emitter.Collision = {lv::ParticleCollision::Kill, 0, 0.5f, lv::ParticleColliderSource::Terrain};
    const auto id = scene.CreateObject();
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter}));
    scene.UpdateParticles(0.75);
    const auto events = scene.PollParticleEvents();
    Check(scene.ParticleCount() == 0 && events.Events.size() == 3 &&
          events.Events[1].Kind == lv::ParticleEventKind::Collision && events.Events[2].Kind == lv::ParticleEventKind::Death &&
          Near(events.Events[1].Position.Y, 0) && Near(static_cast<float>(events.Events[1].Time), 0.5f),
          "terrain collision filters out the cube and publishes contact before kill");
    emitter.Collision = {lv::ParticleCollision::Bounce, 0, 0.5f, lv::ParticleColliderSource::Mesh};
    scene.SetParticleEffect(id, lv::MakeParticleEffect({emitter}));
    scene.SeekParticles(id, 0.5);
    std::vector<lv::ParticleSprite> sprites;
    scene.CollectParticleSprites(sprites);
    Check(sprites.size() == 1 && std::abs(sprites[0].Position.Y - 0.75f) < 0.002f,
          "mesh bounce uses the triangle normal and the original frame remainder");
    scene.SetVisible(mesh, false);
    scene.SeekParticles(id, 0.5);
    scene.CollectParticleSprites(sprites);
    Check(sprites.size() == 1 && Near(sprites[0].Position.Y, 0), "collider cache rebuilds after visibility changes");
}
} // namespace

int main() {
    TimingAndCurves();
    SpacesAndLifetime();
    DeterminismAndBudget();
    ExtendedModules();
    CollisionAndTrails();
    EventRoutesAndGraph();
    SceneSurfaceCollisions();
    return Finish();
}
