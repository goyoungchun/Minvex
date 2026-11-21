#include <minvex/Viewer.h>

#include <charconv>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
std::string Describe(const lv::Error& error) {
    auto message = error.Message;
    if (!error.Hint.empty()) { message += "\n" + error.Hint; }
    return message;
}

lv::ObjectId AddCube(lv::Scene& scene) {
    auto cube = std::make_shared<lv::ModelAsset>();
    cube->Meshes.push_back({"Cube", {{lv::MakeCube(), 0, false}}});
    cube->Parts.push_back({0});
    cube->Materials.push_back({});
    cube->ModelBounds = {{-.5f, -.5f, -.5f}, {.5f, .5f, .5f}};
    return scene.AddModel(cube);
}
}

int main(int argc, char* argv[]) {
    lv::ViewerRunOptions run;
    int positionalCount = argc - 1;
    if (argc >= 3 && std::string_view(argv[argc - 2]) == "--frames") {
        const std::string_view count(argv[argc - 1]);
        const auto [end, error] = std::from_chars(count.data(), count.data() + count.size(), run.FrameLimit);
        if (error != std::errc{} || end != count.data() + count.size()) {
            std::fputs("--frames requires a nonnegative integer (0 = run until closed).\n", stderr);
            return 2;
        }
        positionalCount -= 2;
    }
    if (positionalCount > 1 || (positionalCount == 1 && std::string_view(argv[1]) == "--frames")) {
        std::fputs("Usage: minvex_quickstart [model-file] [--frames N]\n", stderr);
        return 2;
    }

    lv::ViewerOptions options;
    options.Title = "Minvex Quick Start";
    // Keep VSync enabled and let the display drive motion at its refresh rate.
    options.Quality.Overrides.TargetFps = 0;
    lv::Viewer viewer(options);
    lv::ObjectId object;
    if (positionalCount == 1) {
        const auto model = viewer.AddModel(argv[1]);
        if (!model) {
            std::fprintf(stderr, "%s\n", Describe(model.Error()).c_str());
            return 1;
        }
        object = model.Value();
    } else {
        object = AddCube(viewer.GetScene());
    }

    // This callback rotates the whole model using actual elapsed frame time.
    run.OnFrame = [object, angle = 0.0](lv::Viewer& active, const lv::ViewerFrameInfo& frame) mutable {
        angle = std::fmod(angle + frame.DeltaTimeSeconds, 2 * std::numbers::pi);
        const auto halfAngle = static_cast<float>(angle * .5);
        lv::Transform transform;
        transform.Rotation = {0, std::sin(halfAngle), 0, std::cos(halfAngle)};
        const auto moved = active.SetTransform(object, transform);
        if (!moved) { throw std::runtime_error(Describe(moved.Error())); }
    };

    const auto result = viewer.Run(run);
    if (!result) {
        std::fprintf(stderr, "%s\n", Describe(result.Error()).c_str());
        return 1;
    }
    return 0;
}
