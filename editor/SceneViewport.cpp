#include "SceneViewport.h"
#include <algorithm>
#include <sokol_app.h>
#include "renderer/UiRenderer.h"

namespace lv::editor {
namespace {
Result<void> SetLines(detail::RenderSurface& surface, const DebugDraw& lines, const DebugDraw& other, DebugDraw& stored) {
    try {
        auto copy = lines;
        auto combined = lines;
        for (const auto& line : other.Lines()) { combined.DrawLine(line.Start,line.End,line.Color); }
        auto result = surface.SetDebugDraw(combined);
        if (result) { stored = std::move(copy); }
        return result;
    } catch (const std::exception& error) {
        return Result<void>::Failure({ErrorCode::InsufficientResource,error.what(),"Reduce viewport debug lines.",{}});
    }
}
}
Result<void> SceneViewport::SetDebugDraw(const DebugDraw& d) { return SetLines(surface_,d,terrainDraw_,debugDraw_); }
Result<void> SceneViewport::SetTerrainDraw(const DebugDraw& d) { return SetLines(surface_,d,debugDraw_,terrainDraw_); }
void SceneViewport::Resize(int width,int height) {
    width_=std::clamp(width,0,65536);height_=std::clamp(height,0,65536);
    (void)surface_.Resize(scene_ ? *scene_:empty_,width_,height_);
}
void SceneViewport::Render(const Scene& scene) {
    scene_=&scene;(void)surface_.Resize(scene,width_,height_);
    const bool retry=!UiError||failedUiRevision_!=Ui.Revision();surface_.SetUi(retry ? &Ui:nullptr,sapp_dpi_scale());
    try {if (surface_.Render(scene,detail::RenderClockSeconds()) && retry) {UiError.reset();}}
    catch (const detail::UiRasterError& error) {UiError=error.Info;failedUiRevision_=Ui.Revision();surface_.SetUi(nullptr);surface_.Render(scene,detail::RenderClockSeconds(),true,{.025f,.035f,.055f,1},true);}
}
Result<void> SceneViewport::SetQuality(const QualityProfile& profile,const Scene& scene) {
    scene_=&scene;return surface_.ApplyQuality(profile,scene,width_,height_);
}
}
