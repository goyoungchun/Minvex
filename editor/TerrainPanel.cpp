#include "EditorUI.h"

#include <algorithm>
#include <imgui.h>
#include <cmath>

namespace lv::editor {
namespace {
ModelHandle SelectedTerrain(const EditorDocument& document) {
    const auto id = document.Selection();
    if (!id) { return {}; }
    const auto& model = document.GetScene().GetObject(id).Model;
    return model && model->Terrain ? model : ModelHandle{};
}
} // namespace

void EditorUI::ImportTerrain(bool png) {
    const FilePickerRequest request{FilePickerMode::Open,png ? FilePickerFilter::Png : FilePickerFilter::Csv,
        directory_,{},png ? "Open heightmap" : "Open survey grid"};
    const auto selected = picker_ ? picker_(request) : nativeFiles_.Choose(request);
    controls_.Reset();
    if (!Report(selected) || !selected.Value() || selected.Value()->empty()) { return; }
    const auto& path = selected.Value()->front();
    try {
        const auto grid = png ? LoadTerrainHeightmap(path,terrainLayout_.SpacingX,terrainLayout_.SpacingZ,
                                                      mountain_.MaximumHeight,terrainLayout_.Origin) : LoadTerrainCsv(path);
        if (Report(Document.CreateTerrain(grid))) { FrameScene(); }
    } catch (const std::exception& e) {
        ShowError({ErrorCode::InvalidData,e.what(),"Check the terrain input format, spacing and sample count.",path});
    }
}

void EditorUI::TerrainPanel() {
    if (!terrainPanel_) { return; }
    if (ImGui::Begin("Terrain",&terrainPanel_)) {
        ImGui::BeginDisabled(Document.IsEditing());
        ImGui::InputInt("Columns",&terrainColumns_);
        ImGui::InputInt("Rows",&terrainRows_);
        ImGui::InputDouble("X spacing (m)",&terrainLayout_.SpacingX,0,0,"%.4f");
        ImGui::InputDouble("Z spacing (m)",&terrainLayout_.SpacingZ,0,0,"%.4f");
        if (ImGui::Button("Create plane")) {
            try {
                auto layout = terrainLayout_;
                layout.Columns = static_cast<std::size_t>(terrainColumns_);
                layout.Rows = static_cast<std::size_t>(terrainRows_);
                if (Report(Document.CreateTerrain(TerrainGrid(layout)))) { FrameScene(); }
            } catch (const std::exception& e) {
                ShowError({ErrorCode::InvalidArgument,e.what(),"Use positive spacing and a supported grid size.",{}});
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Import CSV")) { ImportTerrain(false); }
        if (ImGui::TreeNode("Heightmap and plane origin")) {
            ImGui::InputDouble("Easting",&terrainLayout_.Origin.Easting);
            ImGui::InputDouble("Northing",&terrainLayout_.Origin.Northing);
            ImGui::InputDouble("Elevation",&terrainLayout_.Origin.Elevation);
            if (ImGui::Button("Import grayscale PNG")) { ImportTerrain(true); }
            ImGui::TreePop();
        }
        ImGui::InputDouble("Maximum height / PNG range (m)",&mountain_.MaximumHeight);
        const auto model = SelectedTerrain(Document);
        ImGui::BeginDisabled(!model);
        ImGui::InputDouble("Ruggedness",&mountain_.Ruggedness,0.05,0,"%.2f");
        ImGui::InputScalar("Seed",ImGuiDataType_U32,&mountain_.Seed);
        if (ImGui::Button("Apply mountain preset")) {
            try {
                auto grid = *model->Terrain;
                grid.GenerateMountain(mountain_);
                if (Report(Document.SetTerrain(Document.Selection(),grid))) { FrameScene(); }
            } catch (const std::exception& e) {
                ShowError({ErrorCode::InvalidArgument,e.what(),"Set a valid height and ruggedness in [0, 1].",{}},Document.Selection());
            }
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (model) {
            ImGui::Text("%.3f x %.3f m",model->Terrain->Width(),model->Terrain->Depth());
            ImGui::BeginDisabled(terrainStroke_);
            ImGui::Combo("Viewport tool",&terrainTool_,"Select / Transform\0Measure\0Raise\0Lower\0Smooth\0");
            if (terrainTool_ >= 2) {
                ImGui::InputDouble("Brush radius (m)",&brushRadius_);
                ImGui::InputDouble(terrainTool_ == 4 ? "Smooth strength [0, 1]" : "Brush height (m)",&brushStrength_);
            }
            ImGui::Checkbox("Contours",&terrainContours_);
            if (terrainContours_) { ImGui::InputDouble("Contour interval (m)",&terrainInterval_,0,0,"%.3f",ImGuiInputTextFlags_EnterReturnsTrue); }
            ImGui::EndDisabled();
            if (terrainMeasured_ == Document.Selection() && !terrainPoints_.empty()) {
                const auto a = model->Terrain->Sample(terrainPoints_[0][0],terrainPoints_[0][1]);
                if (a) { ImGui::Text("A  E %.3f  N %.3f  Elev %.3f m",a->Easting,a->Northing,a->Elevation); }
                if (terrainPoints_.size() == 2) {
                    const auto b = model->Terrain->Sample(terrainPoints_[1][0],terrainPoints_[1][1]);
                    if (b) { ImGui::Text("B  E %.3f  N %.3f  Elev %.3f m",b->Easting,b->Northing,b->Elevation); }
                    if (a && b) {
                        const auto d = MeasureSurvey(*a,*b);
                        ImGui::Text("Horizontal %.3f m | Rise %+.3f m | Straight %.3f m",d.Horizontal,d.ElevationChange,d.Straight);
                    }
                }
                if (ImGui::Button("Clear measurements")) { terrainPoints_.clear(); }
            }
        }
    }
    ImGui::End();
}

void EditorUI::FinishTerrainStroke(bool cancel) {
    if (terrainStroke_ && Document.IsEditing()) { Report(cancel ? Document.CancelEdit() : Document.CommitEdit()); }
    terrainStroke_ = false;
    terrainStrokeObject_ = NoObject;
}

bool EditorUI::TerrainInput(float x,float y,ViewportSize size,bool hovered,bool cancel) {
    const auto model = SelectedTerrain(Document);
    const auto id = Document.Selection();
    if (!terrainPanel_ || !model || terrainTool_ == 0 || (terrainStroke_ && id != terrainStrokeObject_)) {
        FinishTerrainStroke(true);
        return false;
    }
    if (cancel || (terrainStroke_ && ImGui::IsKeyPressed(ImGuiKey_Escape,false))) { FinishTerrainStroke(true); return true; }
    if (terrainStroke_ && !ImGui::IsMouseDown(0)) { FinishTerrainStroke(false); }
    const bool clicked = hovered && ImGui::IsMouseClicked(0) && !Document.IsEditing();
    if (!hovered || (!clicked && !terrainStroke_)) { return true; }
    try {
        const auto& scene = Document.GetScene();
        const ViewportSize output{static_cast<float>(Viewport.OutputWidth()),static_cast<float>(Viewport.OutputHeight())};
        const auto ray = ScreenRay(scene.MainCamera,output,x/size.Width*output.Width,y/size.Height*output.Height);
        const auto hit = scene.PickSurface(ray,id);
        if (!hit) { return true; }
        const auto local = scene.GetWorldTransform(id).InverseAffine().TransformPoint(hit->Position);
        const auto& grid = *model->Terrain;
        const std::array<double,2> point{std::clamp(double(local.X),0.0,grid.Width()),
                                         std::clamp(double(local.Z),0.0,grid.Depth())};
        if (clicked) { ImGui::SetWindowFocus(); }
        if (terrainTool_ == 1) {
            if (terrainMeasured_ != id || terrainPoints_.size() == 2) { terrainPoints_.clear(); }
            terrainMeasured_ = id;
            terrainPoints_.push_back(point);
            return true;
        }
        const double minStep = std::min(grid.Layout().SpacingX,grid.Layout().SpacingZ)*0.25;
        if (terrainStroke_ && std::hypot(lastBrush_[0]-point[0],lastBrush_[1]-point[1]) < minStep) { return true; }
        auto edited = grid;
        const auto mode = static_cast<TerrainBrushMode>(terrainTool_-2);
        const bool changed = edited.ApplyBrush({mode,point[0],point[1],brushRadius_,brushStrength_});
        if (clicked && Report(Document.BeginEdit(),id)) { terrainStroke_ = true; terrainStrokeObject_ = id; }
        if (changed && terrainStroke_ && !Report(Document.SetTerrain(id,edited),id)) { FinishTerrainStroke(true); }
        lastBrush_ = point;
    } catch (const std::exception& e) {
        FinishTerrainStroke(true);
        ShowError({ErrorCode::InvalidArgument,e.what(),"Check the terrain tool settings and viewport.",{}},id);
    }
    return true;
}

void EditorUI::UpdateTerrainContours() {
    const auto& scene = Document.GetScene();
    const auto id = Document.Selection();
    const auto marks = terrainMeasured_ == id ? std::span<const std::array<double,2>>(terrainPoints_)
                                              : std::span<const std::array<double,2>>{};
    auto model = terrainPanel_ && (terrainContours_ || !marks.empty()) ? SelectedTerrain(Document) : ModelHandle{};
    for (auto current = id; model && current; current = scene.GetObject(current).Parent) {
        if (!scene.GetObject(current).Visible) { model.reset(); }
    }
    if (!model && !contourModel_) { return; }
    try {
        const auto world = model ? scene.GetWorldTransform(id) : Mat4::Identity();
        const bool sameInterval = terrainInterval_ == contourInterval_ || (std::isnan(terrainInterval_) && std::isnan(contourInterval_));
        if (model == contourModel_ && (!model || (world.Elements == contourTransform_ && sameInterval &&
            terrainContours_ == contoursApplied_ && std::ranges::equal(marks,markersApplied_)))) { return; }
        contourModel_ = model;
        contourTransform_ = world.Elements;
        contourInterval_ = terrainInterval_;
        contoursApplied_ = terrainContours_;
        markersApplied_.assign(marks.begin(),marks.end());
        DebugDraw lines;
        if (model) {
            const auto& grid = *model->Terrain;
            const auto lift = static_cast<float>(0.005*std::min(grid.Layout().SpacingX,grid.Layout().SpacingZ));
            if (terrainContours_) {
                for (auto line : grid.Contours(terrainInterval_,MaxDebugLines-1024)) {
                    line.Start.Y += lift;
                    line.End.Y += lift;
                    lines.DrawLine(world.TransformPoint(line.Start),world.TransformPoint(line.End),{0.08f,0.12f,0.08f,0.9f});
                }
            }
            const auto radius = static_cast<float>(0.15*std::min(grid.Layout().SpacingX,grid.Layout().SpacingZ));
            std::vector<Vec3> measured;
            for (const auto& xy : marks) {
                if (const auto point = grid.Sample(xy[0],xy[1])) {
                    measured.push_back(world.TransformPoint({static_cast<float>(xy[0]),
                        static_cast<float>(point->Elevation-grid.Layout().Origin.Elevation),static_cast<float>(xy[1])}));
                    lines.DrawSphere(measured.back(),radius,{1,0.65f,0.1f,1},12);
                }
            }
            if (measured.size() == 2) { lines.DrawLine(measured[0],measured[1],{1,0.65f,0.1f,1}); }
        }
        Report(Viewport.SetTerrainDraw(lines),id);
    } catch (const std::exception& e) {
        Report(Viewport.SetTerrainDraw({}),id);
        ShowError({ErrorCode::InvalidArgument,e.what(),"Choose a larger positive contour interval.",{}},id);
    }
}
} // namespace lv::editor
