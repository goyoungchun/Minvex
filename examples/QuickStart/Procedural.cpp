#include "ProceduralScene.h"
#include <minvex/Viewer.h>

#include <charconv>
#include <cstdio>
#include <numbers>
#include <string_view>

int main(int argc, char* argv[]) {
    try {
        lv::ViewerRunOptions run;
        if (argc == 3 && std::string_view(argv[1]) == "--frames") {
            const std::string_view count(argv[2]);
            const auto [end, error] = std::from_chars(count.data(), count.data() + count.size(), run.FrameLimit);
            if (error != std::errc{} || end != count.data() + count.size()) { return 2; }
        } else if (argc != 1) {
            std::fputs("Usage: minvex_quickstart_procedural [--frames N]\n", stderr);
            return 2;
        }

        lv::ViewerOptions options;
        options.Title = "Minvex Procedural Quick Start";
        options.AutoFrame = false;
        lv::Viewer viewer(options);
        ProceduralScene arm;
        arm.Initialize(viewer.GetScene());

        lv::UiElement label;
        label.Text = "Space / Shift+Space | Shoulder: 0 deg";
        label.Layout.Width = 410;
        label.BlocksInput = false;
        const auto labelId = viewer.GetUi().Add(label).Value();

        run.OnInput = [&](lv::Viewer&, const lv::ViewerInputEvent& event) {
            if (event.Type != lv::ViewerInputType::KeyDown || event.Key != lv::ViewerKey::Space) {
                return false;
            }
            if (!event.Repeat) { arm.Turn(event.Shift ? -1.f : 1.f); }
            return true;
        };
        run.OnFrame = [&](lv::Viewer& active, const lv::ViewerFrameInfo& frame) {
            arm.Update(active.GetScene(), frame.DeltaTimeSeconds);
            const auto degrees = std::lround(arm.Angle() * 180 / std::numbers::pi_v<float>);
            const auto text = "Space / Shift+Space | Shoulder: " + std::to_string(degrees) + " deg";
            active.GetUi().SetRuntimeText(labelId, text).Value();
        };

        const auto result = viewer.Run(run);
        if (!result) {
            std::fprintf(stderr, "%s\n%s\n", result.Error().Message.c_str(), result.Error().Hint.c_str());
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
