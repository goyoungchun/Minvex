#include <minvex/Terrain.h>
#include <minvex/Viewer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
template<class T> T Require(lv::Result<T> result) {
    if (!result) { throw std::runtime_error(result.Error().Message); }
    return std::move(result).Value();
}
void Check(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }

class SurveyViewer {
public:
    SurveyViewer(lv::Viewer& viewer, lv::TerrainGrid grid) : viewer_(viewer), grid_(std::move(grid)) {
        terrain_ = viewer.GetScene().AddModel(lv::MakeTerrainModel(grid_));
        lv::UiElement label;
        label.Kind = lv::UiKind::Text;
        label.Layout = {16,16,620,50};
        label.Normal.Fill.fill({0.05f,0.08f,0.12f,0.93f});
        label.BlocksInput = false;
        label_ = Require(viewer.GetUi().Add(label));
        SetView(false);
        Refresh();
    }

    bool Input(const lv::ViewerInputEvent& input) {
        using enum lv::ViewerInputType;
        if (input.Type == KeyDown && !input.Repeat) {
            if (input.Key == lv::ViewerKey::O) { SetView(!top_); return true; }
            if (input.Key == lv::ViewerKey::C) { contours_ = !contours_; Refresh(); return true; }
        }
        if (input.Type == FocusLost) { measuring_ = false; }
        if (input.Button != lv::ViewerMouseButton::Left) { return false; }
        if (input.Type == MouseDown && input.Shift) {
            Require(viewer_.CapturePointer(input.Button));
            measuring_ = true;
            startX_ = input.X;
            startY_ = input.Y;
            return true;
        }
        if (input.Type == MouseUp && measuring_) {
            measuring_ = false;
            if (std::hypot(input.X-startX_,input.Y-startY_) <= 5) {
                const auto hit = Require(viewer_.PickSurface(input.X,input.Y,terrain_));
                if (hit) { if (points_.size() == 2) { points_.clear(); } points_.push_back(*hit); Refresh(); }
            }
            return true;
        }
        return false;
    }

    void Exercise(std::uint64_t frame) {
        if (frame == 0 || frame == 1) {
            SetView(frame == 1);
            const auto target = Require(viewer_.WorldToScreen(viewer_.GetScene().MainCamera.Target));
            lv::ViewerInputEvent resize;
            resize.Type = lv::ViewerInputType::Resized;
            resize.Width = 900;
            resize.Height = 600;
            Require(viewer_.ProcessInput(resize));
            points_.clear();
            Click({5,0,2.5f});
            Click({10,12,2.5f});
            Check(points_.size() == 2,"Both projected slope endpoints must be picked.");
            const auto distance = Distance();
            Check(std::abs(distance.Horizontal-5)<0.0001 && std::abs(distance.ElevationChange-12)<0.0001 &&
                  std::abs(distance.Straight-13)<0.0001,"Unexpected survey slope measurement.");
            const auto center = Require(viewer_.WorldToScreen({2.5f,6,2.5f}));
            const auto world = Require(viewer_.ScreenToWorld(center));
            Check(std::hypot(world.X-2.5f,world.Y-6,world.Z-2.5f)<0.0001,"Screen/world round trip failed.");
            resize.Width = static_cast<int>(std::lround(target.X*2));
            resize.Height = static_cast<int>(std::lround(target.Y*2));
            Require(viewer_.ProcessInput(resize));
        }
        if (frame == 2) {
            const auto info = Require(viewer_.GetRenderDebugInfo());
            for (const auto name : {"Scene","UI overlay"}) {
                Check(std::ranges::any_of(info.Passes,[&](const auto& pass) { return pass.Name == name && pass.DrawCalls > 0; }),
                      "Expected scene and UI rendering in the survey exercise.");
            }
        }
    }

private:
    void SetView(bool top) {
        top_ = top;
        auto camera = viewer_.GetScene().MainCamera;
        const auto [low,high] = std::minmax_element(grid_.Heights().begin(),grid_.Heights().end());
        const float width = static_cast<float>(grid_.Width()), depth = static_cast<float>(grid_.Depth());
        const float span = std::max({width,depth,static_cast<float>(*high-*low)});
        camera.Target = {width/2,static_cast<float>((*low+*high)/2),depth/2};
        camera.Projection = top ? lv::CameraProjection::Orthographic : lv::CameraProjection::Perspective;
        camera.Up = top ? lv::Vec3{0,0,-1} : lv::Vec3{0,1,0};
        camera.Position = top ? lv::Vec3{camera.Target.X,camera.Target.Y+2*span,camera.Target.Z}
                              : lv::Vec3{camera.Target.X-0.9f*span,camera.Target.Y+1.1f*span,camera.Target.Z+1.4f*span};
        camera.OrthographicHeight = 1.5f*span;
        camera.NearPlane = span*0.001f;
        camera.FarPlane = span*6;
        Require(viewer_.SetCamera(camera));
    }
    lv::SurveyPoint Point(std::size_t index) const {
        const auto& hit = points_.at(index);
        return grid_.SampleTriangle(hit.Triangle,hit.Barycentric);
    }
    lv::SurveyDistance Distance() const { return lv::MeasureSurvey(Point(0),Point(1)); }
    void Refresh() {
        lv::DebugDraw lines;
        const auto [low,high] = std::minmax_element(grid_.Heights().begin(),grid_.Heights().end());
        if (contours_) {
            const auto interval = std::max(0.00001,(*high-*low)/10);
            const auto lift = static_cast<float>(std::min(grid_.Layout().SpacingX,grid_.Layout().SpacingZ)*0.005);
            for (auto line : grid_.Contours(interval,lv::MaxDebugLines-128)) {
                line.Start.Y += lift;
                line.End.Y += lift;
                lines.DrawLine(line.Start,line.End,{0.08f,0.13f,0.09f,1});
            }
        }
        const float radius = static_cast<float>(std::max(grid_.Width(),grid_.Depth())*0.012);
        for (const auto& hit : points_) { lines.DrawSphere(hit.Position,radius,{1,0.65f,0.1f,1},12); }
        char label[220];
        if (points_.size() == 2) {
            lines.DrawLine(points_[0].Position,points_[1].Position,{1,0.65f,0.1f,1});
            const auto d = Distance();
            std::snprintf(label,sizeof label,"Horizontal %.3f m    Rise %+.3f m    Straight %.3f m",d.Horizontal,d.ElevationChange,d.Straight);
        } else if (points_.size() == 1) {
            const auto p = Point(0);
            std::snprintf(label,sizeof label,"E %.3f   N %.3f   Elev %.3f m   |   Shift+click second point",p.Easting,p.Northing,p.Elevation);
        } else { std::snprintf(label,sizeof label,"Shift+click two points   |   O switches view   C toggles contours"); }
        Require(viewer_.SetDebugDraw(lines));
        Require(viewer_.GetUi().SetRuntimeText(label_,label));
    }
    void Click(lv::Vec3 world) {
        const auto at = Require(viewer_.WorldToScreen(world));
        lv::ViewerInputEvent input;
        input.Type = lv::ViewerInputType::MouseDown;
        input.Button = lv::ViewerMouseButton::Left;
        input.Shift = true;
        input.X = at.X;
        input.Y = at.Y;
        Require(viewer_.ProcessInput(input));
        input.Type = lv::ViewerInputType::MouseUp;
        Require(viewer_.ProcessInput(input));
    }

    lv::Viewer& viewer_;
    lv::TerrainGrid grid_;
    lv::ObjectId terrain_ = lv::NoObject;
    lv::UiId label_ = lv::NoUi;
    bool top_ = false, measuring_ = false, contours_ = false;
    float startX_ = 0, startY_ = 0;
    std::vector<lv::SurfaceHit> points_;
};
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        std::filesystem::path csv;
        bool exercise = false;
        for (int i=1; i<argc; ++i) {
            const std::filesystem::path arg(argv[i]);
            if (arg == "--exercise") { exercise = true; }
            else if (csv.empty() && !arg.native().starts_with(std::filesystem::path("--").native())) { csv = arg; }
            else { throw std::invalid_argument("Usage: minvex_survey [grid.csv] [--exercise]"); }
        }
        if (exercise && csv.empty()) { throw std::invalid_argument("Pass SurveySlope.csv with --exercise."); }
        lv::TerrainGrid grid = csv.empty() ? lv::TerrainGrid({129,97,1,1,{3500000,7200000,1000}}) : lv::LoadTerrainCsv(csv);
        if (csv.empty()) { grid.GenerateMountain({28,0.8,42}); }
        lv::ViewerOptions options;
        options.Title = "Minvex | Terrain Survey";
        options.AutoFrame = false;
        lv::Viewer viewer(options);
        SurveyViewer survey(viewer,std::move(grid));
        lv::ViewerRunOptions run;
        run.OnInput = [&](lv::Viewer&,const lv::ViewerInputEvent& input) { return survey.Input(input); };
        if (exercise) {
            run.FrameLimit = 3;
            run.OnFrame = [&](lv::Viewer&,const lv::ViewerFrameInfo& frame) { survey.Exercise(frame.FrameNumber); };
        }
        Require(viewer.Run(run));
        if (exercise) { std::puts("Measured horizontal 5 m, elevation change +12 m, straight 13 m."); }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}
