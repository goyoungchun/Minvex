#include "TestHarness.h"
#include "GraphicsTestTarget.h"
#include <cstring>
using namespace test;
int RunUiPixels() {
    try {
        constexpr int W=640,H=360;GraphicsTestTarget target(W,H);
        lv::EmbeddedRenderer renderer;lv::EmbeddedOptions options;options.Quality=lv::MakeQualityProfile(lv::QualityPreset::Low);options.Quality.Overrides.TargetFps=0;options.Quality.Overrides.ResolutionScale=.5f;renderer.Initialize(target.Context(),options).Value();lv::Scene scene;const auto output=target.Target();lv::EmbeddedFrameOptions frame;frame.ClearColor={0,0,0,1};
        auto read=[&] {return target.Pixels(renderer);};
        lv::UiElement e;e.Kind=lv::UiKind::Button;e.Text="Hello 한글";e.Layout={20,20,220,70};e.Normal.Fill.fill({1,0,0,1});e.Normal.Radius.fill(12);const auto id=renderer.GetUi().Add(e).Value();const auto first=renderer.Render(scene,output,frame).Value();Check(first.SceneRendered&&first.RenderWidth==W/2,"UI composes at output resolution with half resolution scene");auto pixels=read();Check(pixels[(40*W+30)*4]>240&&pixels[(40*W+30)*4+1]<10,"UI lands at logical coordinates");Check(pixels[(20*W+20)*4]<10,"rounded corner clips fill");std::size_t white=0;for (int y=20;y<90;++y) {for (int x=20;x<240;++x) {if (pixels[(y*W+x)*4+1]>200) {++white;}}}Check(white>20,"native text survives GPU composite");
        const auto dirty = renderer.GetRenderDebugInfo().Value().Ui;
        Check(dirty.Composited && dirty.BitmapUpdated && dirty.UploadedBytes == W*H*4 && dirty.RasterCpuMs >= 0 && dirty.UploadCpuMs >= 0,
            "UI diagnostics include raster and upload submission cost for a changed bitmap");
        Check(!renderer.Render(scene,output,frame).Value().SceneRendered,"unchanged UI uses on demand cache");
        frame.ForceRedraw = true;
        renderer.Render(scene,output,frame).Value();
        const auto cached = renderer.GetRenderDebugInfo().Value().Ui;
        Check(cached.Composited && !cached.BitmapUpdated && cached.UploadedBytes == 0 && cached.RasterCpuMs == 0 && cached.CanvasRevision == dirty.CanvasRevision,
            "resubmitting a scene reports UI bitmap reuse");
        frame.ForceRedraw = false;
        e.Normal.Fill.fill({0,1,0,1});renderer.GetUi().Set(id,e).Value();Check(renderer.Render(scene,output,frame).Value().SceneRendered,"UI edit invalidates on demand");pixels=read();Check(pixels[(40*W+30)*4+1]>240&&pixels[(40*W+30)*4]<10,"edited UI replaces cached pixels");
        const auto partial = renderer.GetRenderDebugInfo().Value().Ui;
        Check(partial.BitmapUpdated && partial.UploadedBytes > 0 && partial.UploadedBytes < dirty.UploadedBytes / 4,
            "only the changed rectangle is uploaded");
        lv::UiElement alarm;
        alarm.Layout = {20,110,180,30};
        const auto alarmId = renderer.GetUi().Add(alarm).Value();
        frame.ForceRedraw = true;
        for (int tick = 0; tick <= 12; ++tick) {
            const bool reading = tick % 6 == 0;
            if (reading) { renderer.GetUi().SetRuntimeText(id, std::to_string(tick)).Value(); }
            if (tick == 4) { renderer.GetUi().SetRuntimeText(alarmId, "Low water").Value(); }
            renderer.Render(scene, output, frame).Value();
            const auto current = renderer.GetRenderDebugInfo().Value().Ui;
            Check(current.BitmapUpdated == (reading || tick == 4) && (current.UploadedBytes > 0) == current.BitmapUpdated,
                "UI diagnostics distinguish 5 Hz sensor updates and an immediate alarm in a 30 Hz update schedule");
        }
        renderer.GetUi().SetRuntimeText(id, std::nullopt).Value();
        renderer.GetUi().Remove(alarmId).Value();
        frame.ForceRedraw = false;
        // A CPU export can advance the raster cache ahead of the GPU bitmap.
        auto moved=e;
        moved.Layout.X+=300;
        renderer.GetUi().Set(id,moved).Value();
        renderer.GetUi().Rasterize(W,H).Value();
        renderer.Render(scene,output,frame).Value();
        pixels=read();
        Check(pixels[(40*W+30)*4+1]<10 && pixels[(40*W+330)*4+1]>240 &&
              renderer.GetRenderDebugInfo().Value().Ui.UploadedBytes==dirty.UploadedBytes,
            "the GPU fully refreshes when its revision predates a CPU raster export");
        renderer.GetUi().Set(id,e).Value();
        frame.UiScale=2;renderer.Render(scene,output,frame).Value();pixels=read();Check(pixels[(80*W+60)*4+1]>240&&pixels[(25*W+25)*4+1]<10,"DPI changes rendering without changing logical layout");
        frame.UiScale=1.5f;e.Layout.AnchorX=e.Layout.PivotX=1;e.Layout.X=-12;renderer.GetUi().Set(id,e).Value();renderer.Render(scene,output,frame).Value();pixels=read();Check(pixels[(60*W+330)*4+1]>240,"fractional DPI preserves right anchor layout");renderer.GetUi().ProcessInput({lv::UiInputType::Down,330/1.5f,60/1.5f},W/1.5f,H/1.5f).Value();renderer.GetUi().ProcessInput({lv::UiInputType::Up,330/1.5f,60/1.5f},W/1.5f,H/1.5f).Value();Check(renderer.GetUi().PollActivations().Value()==std::vector<lv::UiId>{id},"fractional DPI hit-test matches composed pixels");
        renderer.GetUi().Clear().Value();renderer.Render(scene,output,frame).Value();pixels=read();Check(pixels[(60*W+330)*4+1]<10,"clearing canvas removes stale overlay");
        auto& ui=renderer.GetUi();frame.UiScale=1;lv::UiElement row;row.Kind=lv::UiKind::Panel;row.Layout={20,20,300,120};row.Container.Flow=lv::UiFlow::Row;row.Container.Padding.fill(10);row.Container.GapX=10;row.Container.Overflow=lv::UiOverflow::Clip;const auto parent=ui.Add(row).Value();lv::UiElement cell;cell.Kind=lv::UiKind::Panel;cell.Parent=parent;cell.Layout={0,0,80,80};cell.Normal.Fill.fill({1,0,0,1});ui.Add(cell).Value();cell.Layout.WidthSizing=lv::UiSizing::Fill;cell.Normal.Fill.fill({0,1,0,1});const auto green=ui.Add(cell).Value();renderer.Render(scene,output,frame).Value();pixels=read();Check(pixels[(40*W+40)*4]>240&&pixels[(40*W+130)*4+1]>240,"GPU composition uses resolved row positions and fill size");
        cell.Normal.Fill.fill({0,0,1,1});ui.Set(green,cell).Value();renderer.Render(scene,output,frame).Value();pixels=read();Check(pixels[(40*W+130)*4+2]>240,"layout edit updates cached composite");ui.Undo().Value();renderer.Render(scene,output,frame).Value();pixels=read();Check(pixels[(40*W+130)*4+1]>240&&pixels[(40*W+130)*4+2]<10,"undo invalidates GPU bitmap and restores pixels");ui.Redo().Value();renderer.Render(scene,output,frame).Value();pixels=read();Check(pixels[(40*W+130)*4+2]>240,"redo restores GPU pixels");
        ui.Clear().Value();lv::UiElement rich;rich.Layout={20,20,500,100};rich.Typography.Padding.fill(0);rich.Typography.Vertical=lv::UiAlign::Start;lv::UiTextRun r;r.Text="MMMM ";r.Color=lv::UiColor{1,0,0,1};r.Size=32.0f;lv::UiTextRun b;b.Text="MMMM";b.Color=lv::UiColor{0,0,1,1};b.Size=32.0f;rich.Runs={r,b};ui.Add(rich).Value();renderer.Render(scene,output,frame).Value();pixels=read();int redText=0,blueText=0;for (std::size_t i=0;i<pixels.size();i+=4) {redText+=pixels[i]>80&&pixels[i+1]<10&&pixels[i+2]<10;blueText+=pixels[i+2]>80&&pixels[i]<10&&pixels[i+1]<10;}Check(redText>80&&blueText>80,"per-run colors survive native raster and GPU sRGB composition");
        renderer.Shutdown().Value();Check(!renderer.IsInitialized(),"UI resources release with graphics session");
    }catch (const std::exception& e) {Check(false,e.what());}return Finish();
}
int main(){return GraphicsScope([]{return RunUiPixels();});}
