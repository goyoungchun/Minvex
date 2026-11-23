#include "EditorUI.h"

#include <imgui.h>
#include <cmath>

namespace lv::editor {
void EditorUI::SurfacePanel() {
    if (!surfacePanel_) { surfacePick_ = false; return; }
    std::erase_if(surfaceHits_,[&](const auto& hit) { return !Document.GetScene().Contains(hit.Object); });
    if (ImGui::Begin("Surface Measure",&surfacePanel_)) {
        if (ImGui::Checkbox("Pick points in Viewport",&surfacePick_) && surfacePick_) { terrainTool_ = 0; }
        ImGui::TextWrapped("Click two points on visible geometry. A third hit starts a new measurement. Esc exits the tool.");
        for (std::size_t i=0; i<surfaceHits_.size(); ++i) {
            const auto& hit = surfaceHits_[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton(i == 0 ? "Select A" : "Select B")) { Report(Document.Select(hit.Object)); }
            ImGui::SameLine();
            ImGui::TextWrapped("%s",Document.GetScene().GetObject(hit.Object).Name.c_str());
            ImGui::Text("X %.3f  Y %.3f  Z %.3f m",hit.Position.X,hit.Position.Y,hit.Position.Z);
            ImGui::Text("Part %zu / Primitive %zu / Triangle %zu",hit.Part,hit.Primitive,hit.Triangle);
            ImGui::PopID();
        }
        if (surfaceHits_.size() == 2) {
            const auto a = surfaceHits_[0].Position, b = surfaceHits_[1].Position;
            const auto horizontal = std::hypot(double(b.X)-a.X,double(b.Z)-a.Z);
            const auto rise = double(b.Y)-a.Y;
            ImGui::Separator();
            ImGui::Text("Distance %.3f m",std::hypot(horizontal,rise));
            ImGui::Text("Horizontal %.3f m | Rise %+.3f m",horizontal,rise);
        }
        ImGui::BeginDisabled(surfaceHits_.empty());
        if (ImGui::Button("Clear points")) { surfaceHits_.clear(); }
        ImGui::EndDisabled();
    }
    ImGui::End();
    if (!surfacePanel_) { surfacePick_ = false; }
}

bool EditorUI::SurfaceInput(float x, float y, ViewportSize size, bool hovered, bool blocked) {
    if (!surfacePanel_ || !surfacePick_) { return false; }
    const auto& io = ImGui::GetIO();
    if (!blocked && !uiNumberFocus_ && !io.AppFocusLost && !io.WantTextInput &&
        (hovered || ImGui::IsWindowFocused()) && ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
        surfacePick_ = false;
        status_ = "Surface measurement finished.";
        return true;
    }
    if (!blocked && hovered && !io.AppFocusLost && !io.WantTextInput &&
        !Document.IsEditing() && ImGui::IsMouseClicked(0)) {
        ImGui::SetWindowFocus();
        try {
            if (const auto hit = Viewport.PickSurface(Document.GetScene(),size,x,y)) {
                if (surfaceHits_.size() == 2) { surfaceHits_.clear(); }
                surfaceHits_.push_back(*hit);
                status_ = surfaceHits_.size() == 1 ? "Surface point A set. Pick point B." : "Surface distance measured.";
            } else { status_ = "No surface at the cursor."; }
        } catch (const std::exception& e) {
            ShowError({ErrorCode::InvalidArgument,e.what(),"Check the camera and visible geometry.",{}});
        }
    }
    return true;
}

void EditorUI::DrawSurfaceMarks(float x, float y, ViewportSize size) {
    if (!surfacePanel_ || surfaceHits_.empty()) { return; }
    const auto& camera = Document.GetScene().MainCamera;
    const ViewportSize output{static_cast<float>(Viewport.OutputWidth()),static_cast<float>(Viewport.OutputHeight())};
    std::array<std::optional<ImVec2>,2> points;
    for (std::size_t i=0; i<surfaceHits_.size(); ++i) {
        try {
            const auto point = WorldToScreen(camera,output,surfaceHits_[i].Position);
            if (point.Depth >= 0 && point.Depth <= 1) {
                points[i] = ImVec2{x+point.X*size.Width/output.Width,y+point.Y*size.Height/output.Height};
            }
        } catch (const std::invalid_argument&) {} // The camera may have moved behind a recorded point.
    }
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect({x,y},{x+size.Width,y+size.Height},true);
    constexpr ImU32 color = IM_COL32(255,192,76,255);
    if (points[0] && points[1]) { draw->AddLine(*points[0],*points[1],color,2); }
    for (std::size_t i=0; i<points.size(); ++i) {
        if (!points[i]) { continue; }
        const auto point = *points[i];
        draw->AddCircleFilled(point,7,IM_COL32(25,25,25,255));
        draw->AddCircleFilled(point,4,color);
        draw->AddText({point.x+9,point.y-12},color,i == 0 ? "A" : "B");
    }
    draw->PopClipRect();
}
} // namespace lv::editor
