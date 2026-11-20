#include "NativeReadback.h"
#include "SkinningScene.h"

#include <cstdio>
#include <fstream>

namespace {
constexpr int Width = 640, Height = 360;

template<class T>
T Require(lv::Result<T> result) {
    if (!result) { throw std::runtime_error(result.Error().Message + "\n" + result.Error().Hint); }
    return std::move(result).Value();
}

void SavePpm(const char* path, const std::vector<std::uint8_t>& rgba) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << Width << ' ' << Height << "\n255\n";
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        file.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
    }
    file.close();
    if (!file) { throw std::runtime_error(std::string("Cannot write ") + path); }
}

int CaptureArm() {
    capture::NativeReadback target(Width, Height, true);
    lv::EmbeddedRenderer renderer;
    Require(renderer.Initialize(target.Context()));
    lv::Scene scene;
    const auto arm = skinning::AddArm(scene);
    auto& player = scene.GetAnimation(arm);

    lv::EmbeddedFrameOptions frame;
    frame.ForceRedraw = true;
    frame.ClearColor = {0, 0, 0, 1};
    const auto render = [&] {
        if (!Require(renderer.Render(scene, target.Target(), frame)).Presented) {
            throw std::runtime_error("No frame was rendered into the Host texture.");
        }
        return target.Pixels(renderer);
    };

    player.Play(0);
    const auto straight = render();
    player.Seek(1); // The elbow reaches 90 degrees at the second key.
    const auto bent = render();
    player.Stop();
    if (render() != straight) { throw std::runtime_error("Stop() did not restore the first frame."); }

    std::size_t straightPixels = 0, bentPixels = 0, changedPixels = 0;
    for (std::size_t i = 0; i < straight.size(); i += 4) {
        straightPixels += straight[i] > 32;
        bentPixels += bent[i] > 32;
        changedPixels += (straight[i] > 32) != (bent[i] > 32);
    }
    if (straightPixels < 100 || bentPixels < 100 || changedPixels < 100) {
        throw std::runtime_error("Expected visible arm geometry and a change in silhouette.");
    }
    Require(renderer.Shutdown());
    SavePpm("straight.ppm", straight);
    SavePpm("bent.ppm", bent);
    std::printf("Saved straight.ppm and bent.ppm (%zu pixels changed in the silhouette).\n", changedPixels);
    return 0;
}
} // namespace

int main() {
    return capture::GraphicsScope([] {
        try { return CaptureArm(); }
        catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    });
}
