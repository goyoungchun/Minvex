#include "ProceduralScene.h"
#include <minvex/WebScene.h>

lv::WebSceneCallbacks lv::CreateWebSceneCallbacks() {
    auto arm = std::make_shared<ProceduralScene>();
    return {
        [arm](lv::Scene& scene) { arm->Initialize(scene); },
        [arm](lv::Scene& scene, const lv::ViewerFrameInfo& frame) {
            arm->Update(scene, frame.DeltaTimeSeconds);
        },
        [arm](lv::Scene&, std::string_view name, std::string_view payload) {
            if (name != "turn" || (payload != "left" && payload != "right")) {
                return lv::Result<void>::Failure({lv::ErrorCode::InvalidArgument,
                    "Expected a turn command with left or right.", {}, {}});
            }
            arm->Turn(payload == "left" ? -1.f : 1.f);
            return lv::Result<void>::Success();
        },
    };
}
