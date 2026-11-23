#include "TestHarness.h"

#include <minvex/EditorDocument.h>
#include <minvex/Terrain.h>
#include <minvex/Viewer.h>
#if MINVEX_TEST_BOX_PHYSICS
#include <minvex/Physics.h>
#endif

#include <algorithm>

using namespace test;
namespace {
void PresetAndBrush() {
    lv::TerrainGrid grid({33,25,1.5,2,{3500000,7200000,1000}});
    const lv::MountainPreset settings{28,0.75,17};
    grid.GenerateMountain(settings);
    Check(*std::ranges::max_element(grid.Heights()) == settings.MaximumHeight,"preset peak reaches the specified height");
    auto duplicate = grid;
    duplicate.GenerateMountain(settings);
    Check(std::ranges::equal(grid.Heights(),duplicate.Heights()),"same seed reproduces height samples exactly");
    duplicate.GenerateMountain({28,0.75,18});
    Check(!std::ranges::equal(grid.Heights(),duplicate.Heights()),"different seed changes rugged terrain");
    Check(grid.MakeMesh().Indices.size() == 32*24*6,"terrain mesh includes two triangles per cell");
    const auto original = lv::MakeTerrainModel(grid);
    const auto point = grid.Sample(24,24).value();
    const auto beforeContours = grid.Contours(2);
    Check(grid.ApplyBrush({lv::TerrainBrushMode::Raise,24,24,4,3}),"raise brush edits samples");
    Check(std::abs(grid.Sample(24,24)->Elevation-point.Elevation-3)<1e-10,"the edited point increases by three metres");
    Check(original->Terrain->Sample(24,24)->Elevation == point.Elevation,"published terrain keeps an immutable copy");
    const auto samePoint = [](lv::Vec3 a,lv::Vec3 b) { return a.X == b.X && a.Y == b.Y && a.Z == b.Z; };
    Check(!std::ranges::equal(grid.Contours(2),beforeContours,[&](const auto& a,const auto& b) {
        return samePoint(a.Start,b.Start) && samePoint(a.End,b.End);
    }),"contours follow the brush edit");
    Check(grid.ApplyBrush({lv::TerrainBrushMode::Smooth,24,24,4,1}) && grid.Sample(24,24)->Elevation<point.Elevation+3,
          "smooth brush uses adjacent samples");
    Check(grid.ApplyBrush({lv::TerrainBrushMode::Lower,24,24,4,1}),"lower brush edits samples");
    const auto edited = grid.Sample(24,24)->Elevation;
    Rejects([&] { grid.ApplyBrush({lv::TerrainBrushMode::Raise,24,24,4,1e9}); },"invalid brush rolls back atomically");
    Check(grid.Sample(24,24)->Elevation == edited,"invalid brush preserves grid");
    Check(!grid.Sample(-1,0) && !grid.Sample(0,49),"sample outside grid misses");
}

void ImportsAndPicking() {
    const auto path = std::filesystem::path(MINVEX_SURVEY_FIXTURE);
    const auto terrain = lv::LoadTerrainCsv(path);
    const auto a = terrain.Sample(5,2.5).value(), b = terrain.Sample(10,2.5).value();
    const auto distance = lv::MeasureSurvey(a,b);
    Check(a.Easting == 3500005 && a.Northing == 7200002.5 && a.Elevation == 1000,"CSV preserves double source coordinates and elevation");
    Check(distance.Horizontal == 5 && distance.ElevationChange == 12 && distance.Straight == 13,"survey fixture is a 5 / +12 / 13 metre slope");
    const auto malformed = test::TemporaryDirectory{};
    const auto duplicate = malformed.File("duplicates.csv");
    Write(duplicate,Replace(Read(path),"3500015,7200005,1012","3500010,7200005,1012"));
    Rejects([&] { (void)lv::LoadTerrainCsv(duplicate); },"CSV duplicate grid points fail");

    const auto heightmap = malformed.File("gray16.png");
    constexpr unsigned char png[]{
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x02,0x10,0x00,0x00,0x00,0x00,0x07,0x4d,0x8e,0xbb,
        0x00,0x00,0x00,0x15,0x49,0x44,0x41,0x54,0x78,0x01,0x01,0x0a,0x00,0xf5,0xff,0x00,0x00,
        0x00,0x80,0x00,0x00,0x12,0x34,0xff,0xff,0x07,0x6b,0x02,0xc5,0x46,0x29,0x3a,0x19,0x00,
        0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
    Write(heightmap,std::string(reinterpret_cast<const char*>(png),sizeof png));
    const auto decoded = lv::LoadTerrainHeightmap(heightmap,2,3,65535,{10,20,30});
    Check(decoded.Sample(2,0)->Elevation == 32768+30 && decoded.Sample(0,3)->Elevation == 4660+30,
          "16-bit PNG preserves height values and row order");

    lv::ViewerOptions options;
    options.AutoFrame = false;
    lv::Viewer viewer(options);
    auto& scene = viewer.GetScene();
    const auto parent = scene.CreateObject("Survey origin");
    scene.SetTransform(parent,{{2,3,4},{},{2,0.5f,1}});
    const auto id = scene.AddModel(lv::MakeTerrainModel(terrain),{},parent);
    const auto other = scene.AddModel(lv::MakeTerrainModel(terrain),{{2,10,4},{},{2,1,1}});
    lv::Camera camera;
    camera.Position = {17,40,6.5f};
    camera.Target = {17,6,6.5f};
    camera.Up = {0,0,-1};
    camera.OrthographicHeight = 25;
    for (const auto projection : {lv::CameraProjection::Perspective,lv::CameraProjection::Orthographic}) {
        camera.Projection = projection;
        viewer.SetCamera(camera).Value();
        lv::ViewerInputEvent resize;
        resize.Type = lv::ViewerInputType::Resized;
        resize.Width = 900;
        resize.Height = 475;
        viewer.ProcessInput(resize).Value();
        const auto screen = viewer.WorldToScreen(camera.Target).Value();
        Check(Near(screen.X,450) && Near(screen.Y,237.5f),"resized viewport uses logical pixel coordinates");
        const auto unprojected = viewer.ScreenToWorld(screen).Value();
        Check(Near(unprojected.X,17) && Near(unprojected.Y,6) && Near(unprojected.Z,6.5f),"screen unprojects to world");
        const auto closest = viewer.PickSurface(screen.X,screen.Y).Value();
        const auto hit = viewer.PickSurface(screen.X,screen.Y,id).Value();
        Check(closest && closest->Object == other,"unfiltered surface pick chooses nearest instance");
        Check(hit && hit->Object == id && Near(hit->Position.Y,6) && std::abs(hit->Distance-34)<1e-5,
              "filtered surface pick respects parent transform and reports world distance");
        Check(hit && std::abs(terrain.SampleTriangle(hit->Triangle,hit->Barycentric).Elevation-1006)<1e-5,
              "source triangle weights recover the double elevation");
        scene.SetVisible(parent,false);
        Check(!viewer.PickSurface(screen.X,screen.Y,id).Value(),"hidden ancestor excludes the surface");
        scene.SetVisible(parent,true);
        auto ray = viewer.ScreenRay(screen.X,screen.Y).Value();
        ray.MaxDistance = 30;
        Check(!scene.PickSurface(ray,id),"surface intersection respects the ray range");
    }
    Check(!viewer.PickSurface(-1,15).Value(),"screen position outside viewport misses");
    Check(!viewer.CapturePointer(lv::ViewerMouseButton::Left) && viewer.ReleasePointer(),"capture requires an OnInput button press");
}

void TerrainDocument() {
    lv::EditorDocument document;
    lv::TerrainGrid grid({3,3,2,2,{3500000,7200000,1000}});
    const auto object = document.CreateTerrain(grid).Value();
    auto material = document.GetScene().GetMaterial(object,0);
    material.RoughnessFactor = 0.7f;
    document.SetMaterial(object,0,material).Value();
    document.BeginEdit().Value();
    grid.SetHeight(1,1,5);
    document.SetTerrain(object,grid).Value();
    document.CommitEdit().Value();
    document.Undo().Value();
    Check(document.GetScene().GetObject(object).Model->Terrain->Sample(2,2)->Elevation == 1000,"undo restores terrain height");
    document.Redo().Value();
    const auto temp = test::TemporaryDirectory{};
    const auto path = temp.File("mountain.mvxscene.json");
    document.Save(path).Value();
    lv::EditorDocument reopened;
    reopened.Open(path).Value();
    const auto loaded = reopened.GetScene().ObjectIds().at(0);
    Check(reopened.GetScene().GetObject(loaded).Model->Terrain->Sample(2,2)->Elevation == 1005,"edited terrain is embedded and reopens");
    Check(Near(reopened.GetScene().GetMaterial(loaded,0).RoughnessFactor,0.7f),"terrain edits and archives preserve material override");
}

#if MINVEX_TEST_BOX_PHYSICS
void BoxPhysics() {
    const std::array<lv::PhysicsBox,5> boxes{{
        {{{-3,-1,-3},{3,0,3}},true},
        {{{-1,0,-1},{0.5f,0.25f,1}},true},
        {{{-0.5f,0,-1},{1,0.25f,1}},true},
        {{{-1,0.28f,-1},{1,1.28f,1}},false},
        {{{-0.5f,1.5f,-0.5f},{0.5f,2,0.5f}},false}}};
    auto contact = lv::CheckBoxSupport(boxes,3);
    Check(std::abs(contact.Gap-0.03)<1e-7 && !contact.CenterOverContact,"support measures a 3 cm gap");
    contact = lv::CheckBoxSupport(boxes,3,0.04);
    Check(contact.ContactFraction == 1 && contact.CenterOverContact,"overlapping supports are unioned without double counting");
    const auto preview = lv::PreviewBoxGravity(boxes);
    Check(boxes[3].Collider.Min.Y == 0.28f && preview[3].Box.Collider.Min.Y == 0.25f && preview[3].InContact,
          "gravity settles the lower box without modifying the plan");
    Check(preview[4].InContact && preview[4].Box.Collider.Min.Y == preview[3].Box.Collider.Max.Y,
          "upper box settles on lower box in the same preview");
    auto overlapping = boxes;
    overlapping[3].Collider.Min.Y = 0.2f;
    Rejects([&] { (void)lv::PreviewBoxGravity(overlapping); },"initial intersections are reported");
}
#endif
} // namespace

int main() {
    try {
        PresetAndBrush();
        ImportsAndPicking();
        TerrainDocument();
#if MINVEX_TEST_BOX_PHYSICS
        BoxPhysics();
#endif
    } catch (const std::exception& error) { Check(false,error.what()); }
    return Finish();
}
