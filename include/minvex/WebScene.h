#pragma once

#include <minvex/Viewer.h>
#include <string_view>

namespace lv {

// Optional source extension for the Emscripten Web Host. Callbacks run
// synchronously on the browser main thread with the host-owned Scene.
struct WebSceneCallbacks {
    // Runs once per Viewer before its first render, including on autoStart:false.
    std::function<void(Scene&)> OnInitialize;
    // Runs after animation advancement and before rendering, once per tick.
    std::function<void(Scene&, const ViewerFrameInfo&)> OnFrame;
    // Handles JavaScript viewer.sceneCommand(name, payload). Views are call-local.
    std::function<Result<void>(Scene&, std::string_view name, std::string_view payload)> OnCommand;
};

// Define once in the sources supplied through MINVEX_WEB_SCENE_SOURCES.
// Called per Viewer. Capture state by value to keep instances independent.
WebSceneCallbacks CreateWebSceneCallbacks();

} // namespace lv
