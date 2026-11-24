#include <minvex/Viewer.h>

#include <charconv>
#include <cstdio>
#include <exception>
#include <string_view>

int main(int argc, char** argv) {
    try {
        lv::ViewerRunOptions run;
        if (argc == 3 && std::string_view(argv[1]) == "--frames") {
            const std::string_view value(argv[2]);
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), run.FrameLimit);
            if (error != std::errc{} || end != value.data() + value.size()) { return 2; }
        } else if (argc != 1) {
            std::puts("Usage: minvex_particles [--frames N]");
            return argc == 2 && std::string_view(argv[1]) == "--help" ? 0 : 2;
        }

        lv::ParticleEmitter smoke;
        smoke.Capacity = 160;
        smoke.Spawn.Rate = 45;
        smoke.Initial.Position = {{-0.18f, 0, -0.18f}, {0.18f, 0.15f, 0.18f}};
        smoke.Initial.Velocity = {{-0.28f, 0.5f, -0.28f}, {0.28f, 1.0f, 0.28f}};
        smoke.Initial.Lifetime = {1.4f, 2.6f};
        smoke.Initial.Size = {0.25f, 0.55f};
        smoke.ColorOverLife = {{0, {0.65f, 0.35f, 0.2f, 0.14f}},
                               {0.25f, {0.23f, 0.29f, 0.36f, 0.3f}},
                               {1, {0.07f, 0.1f, 0.16f, 0}}};
        smoke.SizeOverLife = {{0, 0.3f}, {1, 2.5f}};

        lv::ParticleEmitter sparks;
        sparks.Capacity = 384;
        sparks.Blend = lv::ParticleBlend::Additive;
        sparks.Spawn.Rate = 170;
        sparks.Spawn.Bursts = {{0, 90}};
        sparks.Initial.Position = smoke.Initial.Position;
        sparks.Initial.Velocity = {{-1.2f, 1.1f, -1.2f}, {1.2f, 3.1f, 1.2f}};
        sparks.Initial.Lifetime = {0.28f, 1.05f};
        sparks.Initial.Size = {0.03f, 0.08f};
        sparks.Initial.Rotation = {-1.3f, 1.3f};
        sparks.Initial.AngularVelocity = {-1.5f, 1.5f};
        sparks.Acceleration = {0, -2.8f, 0};
        sparks.AspectRatio = 2.5f;
        sparks.ColorOverLife = {{0, {1, 0.84f, 0.46f, 1}}, {0.25f, {1, 0.45f, 0.12f, 0.95f}},
                                {1, {1, 0.07f, 0.02f, 0}}};
        sparks.SizeOverLife = {{0, 0.7f}, {0.2f, 1}, {1, 0.25f}};

        lv::ViewerOptions options;
        options.Title = "Minvex Particles";
        options.AutoFrame = false;
        options.ClearColor = {0.012f, 0.017f, 0.028f, 1};
        options.Quality = lv::MakeQualityProfile(lv::QualityPreset::Low);
        lv::Viewer viewer(options);
        auto& scene = viewer.GetScene();
        scene.MainCamera.Position = {3.6f, 2.25f, 5.8f};
        scene.MainCamera.Target = {0, 1.15f, 0};
        scene.SetParticleEffect(scene.CreateObject("Smoke and sparks"), lv::MakeParticleEffect({smoke, sparks}), 42);

        const auto result = viewer.Run(run);
        if (!result) { std::fprintf(stderr, "%s\n", result.Error().Message.c_str()); return 1; }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
