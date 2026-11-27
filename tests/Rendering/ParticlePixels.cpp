#include "GraphicsTestTarget.h"
#include "TestHarness.h"

using namespace test;

namespace {
void Run() {
    constexpr int extent = 64;
    constexpr int center = 4 * (extent * (extent / 2) + extent / 2);
    GraphicsTestTarget target(extent, extent);
    lv::EmbeddedRenderer renderer;
    lv::EmbeddedOptions options;
    options.Quality = lv::MakeQualityProfile(lv::QualityPreset::Low);
    options.Quality.Overrides.TargetFps = 0;
    options.Quality.Overrides.OnDemand = lv::OnDemandMode::On;
    renderer.Initialize(target.Context(), options).Value();

    lv::ParticleEmitter smoke;
    smoke.Spawn = {0, 0, {{0, 1}}};
    smoke.Initial.Size = {1, 1};
    smoke.ColorOverLife = {{0, {0, 0, 1, 0.5f}}, {1, {0, 0, 1, 0}}};
    auto glow = smoke;
    glow.Blend = lv::ParticleBlend::Additive;
    glow.ColorOverLife = {{0, {0, 1, 0, 1}}, {1, {0, 1, 0, 0}}};
    lv::Scene scene;
    scene.MainCamera.Position = {0, 0, 2};
    scene.MainCamera.Projection = lv::CameraProjection::Orthographic;
    scene.MainCamera.OrthographicHeight = 2;
    const auto emitter = scene.CreateObject();
    const auto diagnostics = [&] { return renderer.GetRenderDebugInfo().Value().ParticleEmitters; };
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({smoke, glow}));
    lv::EmbeddedFrameOptions frame;
    frame.ClearColor = {0, 0, 0, 1};

    renderer.Render(scene, target.Target(), frame).Value();
    Check(scene.ParticleCount() == 0, "Embedded Render leaves the simulation clock unchanged");
    Check(diagnostics().size() == 2 && diagnostics()[0].Path == lv::ParticleRenderPath::None,
          "empty submission describes each emitter without assigning a render path");
    renderer.Update(scene, 0).Value();
    Check(renderer.Render(scene, target.Target(), frame).Value().SceneRendered, "host update fires initial bursts");
    const auto initial = target.Pixels(renderer);
    Check(initial[center + 1] > 100 && initial[center + 2] > 100, "native readback contains both sprite blend modes");
    Check(!renderer.Render(scene, target.Target(), frame).Value().SceneRendered, "on-demand reuses an unchanged frame");
    renderer.Update(scene, 1).Value();
    Check(renderer.Render(scene, target.Target(), frame).Value().SceneRendered, "expiry redraws the host render target");
    const auto expired = target.Pixels(renderer);
    Check(expired[center + 1] == 0 && expired[center + 2] == 0, "no particle pixels remain after expiry");
    smoke.ColorOverLife.clear();
    smoke.Flipbook = {lv::LoadParticleTexture(Fixture("ParticleFlipbook.png")), 2, 2, 1};
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({smoke}));
    scene.SeekParticles(emitter, 0);
    renderer.Render(scene, target.Target(), frame).Value();
    const auto first = target.Pixels(renderer);
    scene.SeekParticles(emitter, 0.55);
    renderer.Render(scene, target.Target(), frame).Value();
    const auto later = target.Pixels(renderer);
    Check(first[center] > 240 && first[center + 1] < 5 && first[center + 2] < 5 &&
          later[center] < 5 && later[center + 1] < 5 && later[center + 2] > 240,
          "two native readbacks sample the expected top-left and bottom-left flipbook frames");
    smoke.Flipbook = {};
    smoke.Initial.Size = {0.2f, 0.2f};
    smoke.Initial.Velocity = {{1, 0, 0}, {1, 0, 0}};
    smoke.Trail = {0.6f, 3, 1};
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({smoke}));
    scene.SeekParticles(emitter, 0.6);
    renderer.Render(scene, target.Target(), frame).Value();
    const auto trail = target.Pixels(renderer);
    const auto trailCenter = 4 * (extent * (extent / 2) + 41);
    Check(trail[trailCenter] > 30, "a trail segment appears between the emitter and the particle");
    Check(diagnostics().at(0).SubmittedParticles == 1, "particle diagnostics count a sprite and its trail once");
    smoke.MaxDrawDistance = 1;
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({smoke}));
    scene.SeekParticles(emitter, 0.6);
    renderer.Render(scene, target.Target(), frame).Value();
    Check(target.Pixels(renderer)[trailCenter] == 0 && scene.ParticleCount() == 1,
          "distance culling removes trail pixels and preserves simulation");
    Check(diagnostics().at(0).SubmittedParticles == 0 && diagnostics().at(0).Path == lv::ParticleRenderPath::None,
          "CPU culling produces zero submitted particles");
    auto shape = smoke;
    shape.MaxDrawDistance = 0;
    shape.Trail = {};
    shape.Initial.Size = {0.45f, 0.45f};
    shape.Initial.Velocity = {};
    shape.Geometry.Type = lv::ParticleRendererType::Mesh;
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({shape}));
    scene.SeekParticles(emitter, 0);
    renderer.Render(scene, target.Target(), frame).Value();
    Check(target.Pixels(renderer)[center] > 100, "the Mesh renderer draws lit cube faces");
    const auto cubePixels = target.Pixels(renderer);
    lv::AssetRegistry registry;
    shape.Geometry.Mesh = registry.LoadModel(Fixture("SharedBox.glb"));
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({shape}));
    scene.SeekParticles(emitter, 0);
    renderer.Render(scene, target.Target(), frame).Value();
    const auto customPixels = target.Pixels(renderer);
    bool customVisible = false;
    for (std::size_t i = 0; i < customPixels.size(); i += 4) { customVisible |= customPixels[i] > 100; }
    Check(customVisible && customPixels != cubePixels, "the Mesh renderer uses the selected model geometry");

    shape.Geometry = {};
    shape.Geometry.Type = lv::ParticleRendererType::Beam;
    shape.Geometry.BeamTarget = {0.7f, 0, -0.25f};
    shape.Initial.Size = {0.12f, 0.12f};
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({shape}));
    scene.SeekParticles(emitter, 0);
    renderer.Render(scene, target.Target(), frame).Value();
    Check(target.Pixels(renderer)[trailCenter] > 150, "the Beam renderer reaches its authored endpoint");

    shape.Geometry.Type = lv::ParticleRendererType::Ribbon;
    shape.Spawn = {4, 0, {{0, 1}}};
    shape.Initial.Velocity = {{1,0,0},{1,0,0}};
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({shape}));
    scene.SeekParticles(emitter, 0.6);
    renderer.Render(scene, target.Target(), frame).Value();
    Check(target.Pixels(renderer)[trailCenter] > 150, "the Ribbon renderer connects successive particles in birth order");

    glow.ColorOverLife = {{0, {0, 1, 0, 1}}};
    glow.Simulation = lv::ParticleSimulation::Gpu;
    glow.Initial.Velocity = {{0.8f,0,0},{0.8f,0,0}};
    glow.Initial.Size = {0.18f,0.18f};
    glow.Drag = 0.5f;
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({glow}));
    scene.SeekParticles(emitter, 0.5);
    renderer.Render(scene, target.Target(), frame).Value();
    const auto gpu = target.Pixels(renderer);
    const auto shaderResources = renderer.GetRenderDebugInfo().Value().Resources;
    const auto gpuCenter = 4 * (extent * (extent / 2) + 43);
    Check(gpu[gpuCenter + 1] > 200 && gpu[center + 1] == 0 &&
          std::any_of(shaderResources.begin(), shaderResources.end(), [](const auto& resource) {
              return resource.Name == "Particle GPU simulation output";
          }), "native compute updates position and supplies the on-GPU instance stream to the draw");
    const auto gpuInfo = diagnostics().at(0);
    Check(gpuInfo.Object == emitter && gpuInfo.Emitter == 0 && gpuInfo.Path == lv::ParticleRenderPath::Gpu &&
          gpuInfo.Fallback == lv::ParticleGpuFallback::None && gpuInfo.SubmittedParticles == 1,
          "the native draw publishes per-emitter compute diagnostics");
    auto png = glow;
    png.Flipbook = {lv::LoadParticleTexture(Fixture("ParticleFlipbook.png")), 2, 2, 1};
    scene.SetParticleEffect(emitter, lv::MakeParticleEffect({glow, png}));
    scene.SeekParticles(emitter, .5);
    renderer.Render(scene, target.Target(), frame).Value();
    const auto mixed = diagnostics();
    Check(mixed.at(0).Path == lv::ParticleRenderPath::Gpu && mixed.at(1).Path == lv::ParticleRenderPath::Cpu &&
          mixed.at(1).Requested == lv::ParticleSimulation::Gpu && mixed.at(1).Fallback == lv::ParticleGpuFallback::Texture &&
          mixed.at(1).SubmittedParticles == 1, "a PNG emitter reports its CPU fallback alongside a GPU emitter");
    scene.SetVisible(emitter, false);
    renderer.Render(scene, target.Target(), frame).Value();
    Check(diagnostics().at(0).Path == lv::ParticleRenderPath::None && diagnostics().at(1).SubmittedParticles == 0,
          "hidden particle effects leave neither stale paths nor submitted counts");
    renderer.Shutdown().Value();
}
} // namespace

int main() {
    return GraphicsScope([] {
        try { Run(); }
        catch (const std::exception& error) { Check(false, error.what()); }
        return Finish();
    });
}
