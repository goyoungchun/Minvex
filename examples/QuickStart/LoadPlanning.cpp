#include <minvex/Physics.h>
#include <minvex/Viewer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>

namespace {
template<class T> T Require(lv::Result<T> result) {
    if (!result) { throw std::runtime_error(result.Error().Message); }
    return std::move(result).Value();
}
void Check(bool test, const char* message) { if (!test) { throw std::runtime_error(message); } }
constexpr int PreviewFrameCount = 150;
constexpr double PreviewStep = 1.0/60;
constexpr double PreviewDuration = PreviewFrameCount*PreviewStep;
constexpr std::size_t OverhangingBox = 2, SupportedBox = 3;

class LoadPlanner {
public:
    explicit LoadPlanner(lv::Viewer& viewer) : viewer_(viewer) {
        lv::Camera camera;
        camera.Position = {5,4,6};
        camera.Target = {0,0.6f,0};
        Require(viewer_.SetCamera(camera));
        lv::DebugOptions debug;
        debug.DepthTest = false;
        Require(viewer_.SetDebugOptions(debug));
        Add({0,-0.15f,0},{3,0.15f,1.7f},0,{0.30f,0.35f,0.40f,1});
        Add({0,0.35f,0},{1.6f,0.35f,1.25f},0,{0.55f,0.37f,0.22f,1});
        Add({1.84f,1.23f,0.15f},{0.5f,0.5f,0.4f},2,{0.95f,0.59f,0.28f,1});
        Add({-0.85f,1.18f,-0.20f},{0.55f,0.45f,0.45f},2,{0.36f,0.68f,0.8f,1});
        selected_ = OverhangingBox;
        previewButton_ = Widget(lv::UiKind::Button,{16,16,174,42},"Preview gravity");
        applyButton_ = Widget(lv::UiKind::Button,{198,16,132,42},"Apply result");
        discardButton_ = Widget(lv::UiKind::Button,{338,16,105,42},"Discard");
        status_ = Widget(lv::UiKind::Text,{16,66,550,40},"");
        Widget(lv::UiKind::Text,{16,110,550,36},"Drag a box | Arrows / WASD move | Q / E lower or raise");
        Refresh();
    }

    bool Input(const lv::ViewerInputEvent& input) {
        using enum lv::ViewerInputType;
        if (input.Type == FocusLost && drag_) {
            Move(selected_,drag_->Position);
            drag_.reset();
            return true;
        }
        if (input.Type == MouseDown && input.Button == lv::ViewerMouseButton::Left && !drag_) {
            const auto hit = Require(viewer_.PickSurface(input.X,input.Y));
            if (!hit) { return false; }
            const auto found = std::ranges::find(objects_,hit->Object);
            if (found == objects_.end()) { return false; }
            const auto index = static_cast<std::size_t>(found-objects_.begin());
            if (boxes_[index].Mass == 0) { return false; }
            Require(viewer_.CapturePointer(input.Button));
            selected_ = index;
            drag_ = Drag{boxes_[index].Position,hit->Position};
            Refresh();
            return true;
        }
        if (input.Type == MouseMove && drag_) {
            const auto ray = Require(viewer_.ScreenRay(input.X,input.Y));
            if (std::abs(ray.Direction.Y) < 1e-5f) { return true; }
            const double distance = (double(drag_->Grab.Y)-ray.Origin.Y)/ray.Direction.Y;
            if (distance < ray.MinDistance || distance > ray.MaxDistance) { return true; }
            auto next = drag_->Position;
            next.X += static_cast<float>(ray.Origin.X+ray.Direction.X*distance-drag_->Grab.X);
            next.Z += static_cast<float>(ray.Origin.Z+ray.Direction.Z*distance-drag_->Grab.Z);
            Move(selected_,next);
            return true;
        }
        if (input.Type == MouseUp && input.Button == lv::ViewerMouseButton::Left && drag_) {
            drag_.reset();
            return true;
        }
        if (input.Type == KeyDown && !drag_ && !input.Shortcut) {
            auto center = boxes_[selected_].Position;
            switch (input.Key) {
            case lv::ViewerKey::Left: case lv::ViewerKey::A: center.X -= 0.05f; break;
            case lv::ViewerKey::Right: case lv::ViewerKey::D: center.X += 0.05f; break;
            case lv::ViewerKey::Up: case lv::ViewerKey::W: center.Z -= 0.05f; break;
            case lv::ViewerKey::Down: case lv::ViewerKey::S: center.Z += 0.05f; break;
            case lv::ViewerKey::Q: center.Y -= 0.03f; break;
            case lv::ViewerKey::E: center.Y += 0.03f; break;
            default: return false;
            }
            Move(selected_,center);
            return true;
        }
        return false;
    }

    void Frame(const lv::ViewerFrameInfo& frame, bool exercise) {
        if (exercise) { Exercise(frame.FrameNumber); return; }
        Buttons();
        AdvancePreview(frame.DeltaTimeSeconds);
    }

private:
    struct Drag { lv::Vec3 Position, Grab; };
    struct Preview {
        std::vector<std::vector<lv::RigidBox>> Frames;
        double Elapsed = 0;
        int Shown = 0;
        bool Finished() const { return Shown == PreviewFrameCount; }
    };
    static lv::Transform Pose(const lv::RigidBox& box) {
        const auto half = box.HalfExtents;
        return {box.Position,box.Rotation,{2*half.X,2*half.Y,2*half.Z}};
    }
    static void DrawBox(lv::DebugDraw& lines,const lv::RigidBox& box,lv::DebugColor color) {
        lines.DrawBox({{-.5f,-.5f,-.5f},{.5f,.5f,.5f}},color,Pose(box).Matrix());
    }
    void Add(lv::Vec3 center,lv::Vec3 half,float mass,lv::UiColor color) {
        auto model = std::make_shared<lv::ModelAsset>();
        model->ModelBounds = {{-.5f,-.5f,-.5f},{.5f,.5f,.5f}};
        model->Meshes.push_back({"Box",{{lv::MakeCube(),0,false}}});
        model->Parts.push_back({0});
        model->Materials.emplace_back().BaseColor = color;
        lv::ValidateModel(*model);
        const lv::RigidBox box{center,{},half,mass};
        objects_.push_back(viewer_.GetScene().AddModel(model,Pose(box)));
        boxes_.push_back(box);
    }
    lv::UiId Widget(lv::UiKind kind,lv::UiLayout layout,const char* text) {
        lv::UiElement element;
        element.Kind = kind;
        element.Layout = layout;
        element.Text = text;
        element.BlocksInput = kind == lv::UiKind::Button;
        element.Normal.Fill.fill({0.06f,0.12f,0.19f,0.95f});
        if (kind == lv::UiKind::Button) {
            element.Typography.Wrap = false;
            element.Typography.Horizontal = lv::UiAlign::Center;
            element.Disabled = element.Normal;
            element.Disabled->Text = {0.55f,0.63f,0.70f,1};
        }
        return Require(viewer_.GetUi().Add(element));
    }
    void Move(std::size_t index,lv::Vec3 center) {
        auto moved = boxes_[index];
        moved.Position = center;
        Require(viewer_.SetTransform(objects_[index],Pose(moved)));
        boxes_[index] = moved;
        preview_.reset();
        Refresh();
    }
    void StartPreview() {
        Preview preview;
        preview.Frames.reserve(PreviewFrameCount+1);
        preview.Frames.push_back(boxes_);
        for (int frame=0; frame<PreviewFrameCount; ++frame) {
            auto next = lv::StepRigidBoxes(preview.Frames.back(),{PreviewStep});
            preview.Frames.push_back(std::move(next.Boxes));
        }
        preview_ = std::move(preview);
        Refresh();
    }
    void AdvancePreview(double seconds) {
        if (!preview_ || preview_->Finished()) { return; }
        preview_->Elapsed = std::min(PreviewDuration,preview_->Elapsed+seconds);
        const auto frame = std::min(PreviewFrameCount,static_cast<int>(preview_->Elapsed/PreviewStep+1e-9));
        if (frame == preview_->Shown) { return; }
        preview_->Shown = frame;
        Refresh();
    }
    void ApplyPreview() {
        auto poses = preview_->Frames.back();
        auto& scene = viewer_.GetScene();
        for (std::size_t i=0; i<poses.size(); ++i) {
            Check(scene.Contains(objects_[i]),"The preview no longer matches the loading plan.");
            (void)Pose(poses[i]).Matrix().NormalMatrix();
        }
        for (std::size_t i=0; i<poses.size(); ++i) {
            if (poses[i].Mass == 0) { continue; }
            scene.SetTransform(objects_[i],Pose(poses[i]));
            // A plan stores poses. Replaying a plan starts from rest.
            poses[i].LinearVelocity = {};
            poses[i].AngularVelocity = {};
        }
        boxes_ = std::move(poses);
        preview_.reset();
        Refresh();
    }
    void Buttons() {
        if (drag_) { return; }
        for (const auto id : Require(viewer_.GetUi().PollActivations())) {
            if (id == previewButton_) {
                StartPreview();
            } else if (id == applyButton_ && preview_ && preview_->Finished()) {
                ApplyPreview();
            } else if (id == discardButton_ && preview_) {
                preview_.reset();
                Refresh();
            }
        }
    }
    lv::BoxSupport Support(std::size_t index) const {
        std::vector<lv::PhysicsBox> bounds;
        bounds.reserve(boxes_.size());
        for (const auto& box : boxes_) {
            const auto h = box.HalfExtents;
            const auto p = box.Position;
            bounds.push_back({{{p.X-h.X,p.Y-h.Y,p.Z-h.Z},{p.X+h.X,p.Y+h.Y,p.Z+h.Z}},box.Mass == 0});
        }
        return lv::CheckBoxSupport(bounds,index);
    }
    void Enable(lv::UiId id,bool enabled) {
        auto button = Require(viewer_.GetUi().Get(id));
        if (button.Enabled != enabled) { button.Enabled = enabled; Require(viewer_.GetUi().Set(id,button)); }
    }
    void Refresh() {
        char message[180];
        if (preview_) {
            if (preview_->Finished()) { std::snprintf(message,sizeof message,"Cyan shows the result. Apply or discard it."); }
            else { std::snprintf(message,sizeof message,"Cyan shows gravity playback   %.1f / %.1f s",preview_->Elapsed,PreviewDuration); }
        } else if (std::ranges::any_of(boxes_,[](const auto& box) { return std::abs(box.Rotation.W) < 0.99999f; })) {
            std::snprintf(message,sizeof message,"Preview gravity to inspect contact on rotated boxes.");
        } else {
            const auto support = Support(selected_);
            if (!std::isfinite(support.Gap)) { std::snprintf(message,sizeof message,"No support beneath the selected box"); }
            else if (support.ContactFraction == 0) { std::snprintf(message,sizeof message,"Selected box is %.1f cm above support",100*support.Gap); }
            else if (!support.CenterOverContact) { std::snprintf(message,sizeof message,"Contact covers %.0f%%. Center is outside contact.",100*support.ContactFraction); }
            else { std::snprintf(message,sizeof message,"Contact covers %.0f%% of the selected box",100*support.ContactFraction); }
        }
        Enable(applyButton_,preview_ && preview_->Finished());
        Enable(discardButton_,preview_.has_value());
        Require(viewer_.GetUi().SetRuntimeText(status_,message));
        lv::DebugDraw lines;
        DrawBox(lines,boxes_[selected_],{1,0.8f,0.3f,1});
        if (preview_) {
            for (const auto& box : preview_->Frames[preview_->Shown]) {
                if (box.Mass != 0) { DrawBox(lines,box,{0.1f,0.9f,1,1}); }
            }
        }
        Require(viewer_.SetDebugDraw(lines));
    }
    void Exercise(std::uint64_t frame) {
        if (frame == 0) {
            const auto original = boxes_[OverhangingBox].Position;
            const auto at = Require(viewer_.WorldToScreen(original));
            Send(lv::ViewerInputType::MouseDown,at.X,at.Y);
            Check(Require(viewer_.GetPointerCapture()) == lv::ViewerMouseButton::Left,"Box must own the drag.");
            Send(lv::ViewerInputType::MouseMove,60,30);
            Send(lv::ViewerInputType::MouseUp,60,30);
            Check(!drag_ && Require(viewer_.GetPointerCapture()) == lv::ViewerMouseButton::None &&
                  Require(viewer_.GetUi().PollActivations()).empty(),"Captured release must not activate the preview button.");
            Move(OverhangingBox,original);
            Send(lv::ViewerInputType::MouseDown,at.X,at.Y);
            Send(lv::ViewerInputType::FocusLost,at.X,at.Y);
            Check(!drag_ && Require(viewer_.GetPointerCapture()) == lv::ViewerMouseButton::None,"Focus loss must cancel the drag.");
            Check(std::abs(Support(SupportedBox).Gap-0.03)<1e-5,"The upright box starts 3 cm above the pallet.");
            exercisePlan_ = boxes_;
            Click(60);
            Check(preview_ && preview_->Shown == 0,"Preview button must start playback.");
        }
        if (frame == 1) {
            AdvancePreview(0.55);
            const auto& tipping = preview_->Frames[preview_->Shown][OverhangingBox];
            Check(std::abs(tipping.Rotation.Z) > 0.15f,"The preview must show the overhanging box tilt.");
        }
        if (frame == 2) {
            CheckPreviewRendered();
            AdvancePreview(0.55);
            const auto& falling = preview_->Frames[preview_->Shown][OverhangingBox];
            Check(falling.Position.X > 2 && falling.Position.Y < 0.60f,"The preview must show the box drop onto the deck.");
        }
        if (frame == 3) {
            CheckPreviewRendered();
            Click(350);
            Check(!preview_,"Discard must close the preview.");
            Click(60);
            Click(210);
            Check(preview_ && preview_->Shown == 0,"Apply must wait until playback finishes.");
        }
        if (frame == 4) {
            AdvancePreview(PreviewDuration);
            Check(preview_ && preview_->Finished(),"The final pose must stay visible for review.");
            const auto& final = preview_->Frames.back();
            Check(std::abs(final[SupportedBox].Position.Y-1.15f)<0.003f && std::abs(final[OverhangingBox].Rotation.Z)>0.5f,
                  "Preview result must show the seated box and the turned box.");
        }
        if (frame == 5) {
            CheckPreviewRendered();
            const auto expected = preview_->Frames.back();
            Click(210);
            Check(!preview_,"Apply must close the preview.");
            CheckPlacement(expected);
        }
        if (frame < 5) { CheckPlacement(exercisePlan_); }
    }
    void CheckPlacement(const std::vector<lv::RigidBox>& expected) const {
        for (std::size_t i=0; i<expected.size(); ++i) {
            const auto matrix = Pose(expected[i]).Matrix().Elements;
            Check(Pose(boxes_[i]).Matrix().Elements == matrix &&
                viewer_.GetScene().GetObject(objects_[i]).LocalTransform.Matrix().Elements == matrix,
                "The plan and rendered objects must match the expected placement.");
        }
    }
    void CheckPreviewRendered() const {
        const auto info = Require(viewer_.GetRenderDebugInfo());
        for (const auto name : {"Scene","UI overlay"}) {
            Check(std::ranges::any_of(info.Passes,[&](const auto& pass) { return pass.Name == name && pass.DrawCalls > 0; }),
                  "Expected scene and UI rendering in the loading exercise.");
        }
        Check(info.LineCount==36 && info.LineDrawCalls>0,"The solid plan and two preview outlines must be rendered together.");
    }
    void Click(float x) {
        Send(lv::ViewerInputType::MouseDown,x,30);
        Send(lv::ViewerInputType::MouseUp,x,30);
        Buttons();
    }
    void Send(lv::ViewerInputType type,float x,float y) {
        lv::ViewerInputEvent event;
        event.Type = type;
        event.Button = lv::ViewerMouseButton::Left;
        event.X = x;
        event.Y = y;
        Require(viewer_.ProcessInput(event));
    }

    lv::Viewer& viewer_;
    std::vector<lv::ObjectId> objects_;
    std::vector<lv::RigidBox> boxes_, exercisePlan_;
    std::optional<Preview> preview_;
    std::optional<Drag> drag_;
    std::size_t selected_ = 0;
    lv::UiId previewButton_ = lv::NoUi, applyButton_ = lv::NoUi, discardButton_ = lv::NoUi, status_ = lv::NoUi;
};
} // namespace

int main(int argc,char** argv) {
    try {
        const bool exercise = argc == 2 && std::string_view(argv[1]) == "--exercise";
        if (argc > 1 && !exercise) { throw std::invalid_argument("Usage: minvex_load_planning [--exercise]"); }
        lv::ViewerOptions options;
        options.Title = "Minvex | Load Planning";
        options.AutoFrame = false;
        lv::Viewer viewer(options);
        LoadPlanner planner(viewer);
        lv::ViewerRunOptions run;
        std::uint64_t frames = 0;
        run.FrameLimit = exercise ? 6 : 0;
        run.OnInput = [&](lv::Viewer&,const lv::ViewerInputEvent& input) { return planner.Input(input); };
        run.OnFrame = [&](lv::Viewer&,const lv::ViewerFrameInfo& frame) { planner.Frame(frame,exercise); ++frames; };
        Require(viewer.Run(run));
        if (exercise) {
            Check(frames == run.FrameLimit,"The loading exercise closed before all verification frames ran.");
            std::puts("Verified the pallet tilt and fall, unchanged plan on preview and discard, and explicit pose application.");
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}
