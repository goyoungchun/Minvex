#include "SkinningScene.h"
#include <minvex/Viewer.h>

#include <charconv>
#include <cstdio>
#include <string_view>

int main(int argc, char* argv[]) {
    try {
        lv::ViewerRunOptions run;
        if (argc == 3 && std::string_view(argv[1]) == "--frames") {
            const std::string_view count(argv[2]);
            const auto [end, error] = std::from_chars(count.data(), count.data() + count.size(), run.FrameLimit);
            if (error != std::errc{} || end != count.data() + count.size() || !run.FrameLimit) { return 2; }
        } else if (argc != 1) {
            std::fputs("Usage: minvex_quickstart_skinning [--frames N]\n", stderr);
            return 2;
        }

        lv::ViewerOptions options;
        options.Title = "Minvex Skinning Quick Start";
        options.AutoFrame = false;
        lv::Viewer viewer(options);
        const auto arm = skinning::AddArm(viewer.GetScene());
        viewer.PlayAnimation(arm, 0).Value();

        lv::UiElement label;
        label.Text = "Space: play  |  Shift+Space: stop";
        label.Layout.Width = 350;
        label.BlocksInput = false;
        viewer.GetUi().Add(label).Value();

        // Viewer advances the animation automatically before rendering each frame.
        run.OnInput = [arm](lv::Viewer& active, const lv::ViewerInputEvent& event) {
            if (event.Type != lv::ViewerInputType::KeyDown || event.Key != lv::ViewerKey::Space) {
                return false;
            }
            if (!event.Repeat) {
                if (event.Shift) { active.StopAnimation(arm).Value(); }
                else { active.PlayAnimation(arm, 0).Value(); }
            }
            return true;
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
