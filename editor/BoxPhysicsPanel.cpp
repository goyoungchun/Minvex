#include "EditorUI.h"

#include <imgui.h>
#include <cmath>

namespace lv::editor {
namespace {
bool EditVector(const char* label, Vec3& value) {
    float xyz[]{value.X,value.Y,value.Z};
    if (!ImGui::DragFloat3(label,xyz,0.05f)) { return false; }
    value = {xyz[0],xyz[1],xyz[2]};
    return true;
}
}
void EditorUI::UpdateBoxOverlay() {
    if (!boxPhysics_ || !boxPreview_) {
        if (physicsOverlay_ && Report(Viewport.SetPhysicsDraw({}))) { physicsOverlay_ = false; }
        return;
    }
    DebugDraw lines;
    constexpr DebugColor fixed{0.40f,0.70f,1.00f,1.0f}, moving{1.0f,0.70f,0.20f,1.0f}, contact{0.35f,0.95f,0.50f,1.0f};
    if (boxPreview_->Mode == BoxPreviewMode::Rigid) {
        for (const auto& box : boxPreview_->Rigid.Boxes) {
            const auto size = box.HalfExtents;
            lines.DrawBox({{-size.X,-size.Y,-size.Z},size},box.Mass == 0 ? fixed : moving,
                Mat4::Translation(box.Position)*Mat4::Rotation(box.Rotation));
        }
    } else {
        for (std::size_t i=0; i<boxPreview_->AxisAligned.size(); ++i) {
            const auto& box = boxPreview_->AxisAligned[i];
            const bool resting = boxPreview_->Grounded.empty() ? boxPreview_->Support[i].ContactFraction > 0 : boxPreview_->Grounded[i];
            lines.DrawBox(box.Collider,box.Static ? fixed : resting ? contact : moving);
        }
    }
    if (Report(Viewport.SetPhysicsDraw(lines))) { physicsOverlay_ = true; }
    else if (physicsOverlay_ && Report(Viewport.SetPhysicsDraw({}))) { physicsOverlay_ = false; }
}

void EditorUI::BoxPhysicsPanel() {
    const auto& scene = Document.GetScene();
    if (std::erase_if(boxColliders_,[&](const auto& entry) { return !scene.Contains(entry.Object); })) { boxPreview_.reset(); }
    if (!boxPhysics_) { boxPreview_.reset(); UpdateBoxOverlay(); return; }

    bool refresh = false;
    try {
        if (boxPreview_ && !boxPreview_->Matches(scene)) {
            boxPreview_.reset();
            refresh = true;
            status_ = "Collider sources changed. Run the preview again.";
        }
    } catch (const std::exception& e) {
        boxPreview_.reset(); refresh = true;
        ShowError({ErrorCode::InvalidArgument,e.what(),"Check the collider transforms.",{}});
    }
    if (ImGui::Begin("Box Physics",&boxPhysics_)) {
        const auto selected = Document.Selection();
        const auto existing = std::ranges::find(boxColliders_,selected,&BoxCollider::Object);
        const bool canAdd = selected && scene.GetObject(selected).Model && existing == boxColliders_.end() && boxColliders_.size() < MaxPhysicsBoxes;
        ImGui::TextWrapped("Use the selected model's bounds as a collider. Preview wireframes, then apply the result to the scene.");
        ImGui::BeginDisabled(!canAdd || Document.IsEditing());
        if (ImGui::Button("Add selected collider")) {
            boxColliders_.push_back({selected,bool(scene.GetObject(selected).Model->Terrain)});
            boxPreview_.reset(); refresh = true;
        }
        ImGui::SetItemTooltip("Select a model in Hierarchy or Viewport. Terrain is added as static.");
        ImGui::EndDisabled();
        ImGui::TextDisabled("%zu / %zu colliders",boxColliders_.size(),MaxPhysicsBoxes);
        std::optional<std::size_t> remove;
        for (std::size_t i=0; i<boxColliders_.size(); ++i) {
            auto& collider = boxColliders_[i];
            const auto& object = scene.GetObject(collider.Object);
            ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(collider.Object)));
            if (ImGui::TreeNode("Collider","%s%s",object.Name.c_str(),object.Id == selected ? " (selected)" : "")) {
                if (ImGui::SmallButton("Select")) { Report(Document.Select(collider.Object)); }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) { remove = i; }
                bool changed = ImGui::Checkbox("Static",&collider.Static);
                if (rigidBoxes_) {
                    ImGui::BeginDisabled(collider.Static);
                    changed = ImGui::DragFloat("Mass (kg)",&collider.Mass,0.05f,.001f,1000000,"%.3f",ImGuiSliderFlags_AlwaysClamp) || changed;
                    if (ImGui::TreeNode("Initial velocity")) {
                        changed = EditVector("Linear (m/s)",collider.LinearVelocity) || changed;
                        changed = EditVector("Angular (rad/s)",collider.AngularVelocity) || changed;
                        ImGui::TreePop();
                    }
                    ImGui::EndDisabled();
                    changed = ImGui::DragFloat("Friction",&collider.Friction,.01f,0,10,"%.2f",ImGuiSliderFlags_AlwaysClamp) || changed;
                }
                if (changed) { boxPreview_.reset(); refresh = true; }
                if (boxPreview_ && !boxPreview_->Support.empty()) {
                    const auto& support = boxPreview_->Support[i];
                    if (std::isfinite(support.Gap)) { ImGui::Text("Support gap %.4f m",support.Gap); }
                    else { ImGui::TextDisabled("No support below this box"); }
                    ImGui::Text("Contact %.1f%% | Center %s contact",support.ContactFraction*100,
                        support.CenterOverContact ? "over" : "outside");
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (remove) {
            boxColliders_.erase(boxColliders_.begin()+static_cast<std::ptrdiff_t>(*remove));
            boxPreview_.reset(); refresh = true;
        }
        ImGui::Separator();
        int mode = rigidBoxes_ ? 1 : 0;
        if (ImGui::Combo("Mode",&mode,"Vertical gravity\0Oriented rigid boxes\0")) {
            rigidBoxes_ = mode == 1; boxPreview_.reset(); refresh = true;
        }
        ImGui::TextWrapped("%s",rigidBoxes_ ? "Oriented boxes can rotate and collide with friction." : "Axis-aligned boxes fall along world -Y.");
        auto& seconds = rigidBoxes_ ? rigidSeconds_ : gravitySeconds_;
        const double zero = 0, maximum = rigidBoxes_ ? 1 : 60;
        bool changed = ImGui::DragScalar(rigidBoxes_ ? "Step time (s)" : "Fall time (s)",ImGuiDataType_Double,&seconds,0.01f,&zero,&maximum,"%.3f",ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::TreeNode("Simulation settings")) {
            if (rigidBoxes_) { EditVector("Gravity (m/s²)",boxOptions_.Gravity); }
            else {
                changed = ImGui::InputDouble("Acceleration (m/s²)",&boxOptions_.Acceleration,0,0,"%.5f") || changed;
                changed = ImGui::InputDouble("Support tolerance (m)",&boxOptions_.SupportTolerance,0,0,"%.4f") || changed;
            }
            ImGui::TreePop();
        }
        if (changed && !rigidBoxes_) { boxPreview_.reset(); refresh = true; }
        bool run = false;
        auto previewMode = rigidBoxes_ ? BoxPreviewMode::Rigid : BoxPreviewMode::Gravity;
        ImGui::BeginDisabled(boxColliders_.empty() || Document.IsEditing());
        if (!rigidBoxes_) {
            if (ImGui::Button("Check support")) { previewMode = BoxPreviewMode::Support; run = true; }
            ImGui::SameLine();
        }
        if (ImGui::Button("Run preview")) { run = true; }
        ImGui::EndDisabled();
        const bool advance = rigidBoxes_ && boxPreview_ && boxPreview_->Mode == BoxPreviewMode::Rigid;
        bool step = false;
        if (advance) {
            ImGui::SameLine(); ImGui::BeginDisabled(Document.IsEditing());
            step = ImGui::Button("Next step"); ImGui::EndDisabled();
        }
        if (run || step) {
            try {
                boxOptions_.Seconds = seconds;
                if (step) { boxPreview_->StepRigid({seconds,boxOptions_.Gravity}); }
                else { boxPreview_ = PreviewBoxes(scene,boxColliders_,previewMode,boxOptions_); }
                refresh = true;
                status_ = "Box physics preview ready.";
            } catch (const std::exception& e) {
                ShowError({ErrorCode::InvalidArgument,e.what(),"Check collider bounds, hierarchy and preview time.",{}},selected);
            }
        }
        if (boxPreview_) {
            if (rigidBoxes_) { ImGui::Text("%.3f s | %zu contact points",boxPreview_->Seconds,boxPreview_->Rigid.Contacts.size()); }
            else if (boxPreview_->Mode == BoxPreviewMode::Gravity) { ImGui::Text("Simulated %.3f s",boxPreview_->Seconds); }
            ImGui::TextDisabled("Blue: static | Orange: moving%s",rigidBoxes_ ? "" : " | Green: supported");
            ImGui::BeginDisabled(!boxPreview_->HasChanges() || Document.IsEditing());
            const bool apply = ImGui::Button("Apply transforms");
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Apply all moving transforms in one Undo step.");
            ImGui::SameLine();
            const bool reset = ImGui::Button("Clear preview");
            if (reset || (apply && Report(ApplyBoxPreview(Document,*boxPreview_),selected))) {
                boxPreview_.reset(); refresh = true;
                if (apply) { status_ = "Applied box physics transforms."; }
            }
        }
    }
    ImGui::End();
    try {
        if (refresh || !boxPreview_ || !boxPhysics_) { UpdateBoxOverlay(); }
    } catch (const std::exception& e) {
        boxPreview_.reset();
        UpdateBoxOverlay();
        ShowError({ErrorCode::InvalidArgument,e.what(),"Check the box preview geometry.",{}});
    }
}
} // namespace lv::editor
