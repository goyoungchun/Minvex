#include <minvex/Viewer.h>

#include <charconv>
#include <cmath>
#include <cstdio>
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
}

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::fprintf(stderr, "Usage: minvex_quickstart model.glb [--frames N]\n");
        return 2;
    }

    lv::ViewerRunOptions run;
    if (argc == 4) {
        const std::string_view count(argv[3]);
        const auto [end, error] = std::from_chars(count.data(), count.data() + count.size(), run.FrameLimit);
        if (std::string_view(argv[2]) != "--frames" || error != std::errc{} || end != count.data() + count.size()) {
            std::fputs("--frames requires a nonnegative integer (0 = run until closed).\n", stderr);
            return 2;
        }
    }

    lv::ViewerOptions options;
    options.Title = "Minvex Quick Start";
    // Keep VSync enabled and let the display drive motion at its refresh rate.
    options.Quality.Overrides.TargetFps = 0;
    lv::Viewer viewer(options);
    const auto model = viewer.AddModel(argv[1]);
    if (!model) {
        std::fprintf(stderr, "%s\n", Describe(model.Error()).c_str());
        return 1;
    }

    // The Viewer advances clips started with PlayAnimation before OnFrame.
    // This callback rotates the whole model using actual elapsed frame time.
    run.OnFrame = [object = model.Value(), angle = 0.0](lv::Viewer& active, const lv::ViewerFrameInfo& frame) mutable {
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
