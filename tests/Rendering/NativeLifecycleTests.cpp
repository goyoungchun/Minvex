#include "TestHarness.h"
#include "GraphicsTestTarget.h"
using namespace test;
namespace {
int Run() {
    try {
        GraphicsTestTarget target(96,64);lv::EmbeddedRenderer renderer;lv::Scene scene;
        auto model=std::make_shared<lv::ModelAsset>();model->Meshes.push_back({"Lifecycle",{{lv::MakeCube(),0,false}}});model->Parts.push_back({0});model->Materials.push_back({});model->ModelBounds={{-.5f,-.5f,-.5f},{.5f,.5f,.5f}};scene.AddModel(model);
        scene.MainCamera.Position={0,0,3};scene.MainCamera.Target={};scene.Sun.Intensity=scene.Sun.Ambient=0;scene.Ambient.Intensity=.2f;
        lv::PointLight point;point.Position={1,1,2};point.Range=8;point.Shadow.Enabled=true;scene.PointLights={point};scene.LocalShadowResolution=128;
        lv::RectLight rect;rect.Position={0,2,2};rect.Direction={0,-1,-1};scene.RectLights={rect};
        auto quality=lv::MakeQualityProfile(lv::QualityPreset::Low);quality.Overrides.Lighting=lv::LightingMode::Advanced;quality.Overrides.Shadow=lv::ShadowMode::Basic;quality.Overrides.TargetFps=0;quality.Overrides.OnDemand=lv::OnDemandMode::On;
        lv::EmbeddedOptions options;options.Quality=quality;lv::EmbeddedFrameOptions frame;frame.ClearColor={0,0,0,1};
        std::uint64_t baseline=0;std::vector<std::uint8_t> reference;
        for(unsigned cycle=0;cycle<10;++cycle){GraphicsScope([&]{
            renderer.GetUi().Clear().Value();renderer.Initialize(target.Context(),options).Value();
            const auto render=[&]{return renderer.Render(scene,target.Target(),frame).Value();};render();renderer.WaitIdle().Value();
            const auto pixels=target.Pixels(renderer);const auto bytes=renderer.GetTelemetry().Value().Resident.TotalBytes;
            if(!cycle){baseline=bytes;reference=pixels;}
            Check(bytes==baseline&&pixels==reference,"reinitialized session returns to the same logical resource cost and pixels");
            Check(!render().SceneRendered,"unchanged session image stays cached");
            auto scaled=quality;scaled.Overrides.ResolutionScale=.5f;renderer.SetQuality(scaled,scene).Value();Check(render().RenderWidth==48,"quality resize uses the reduced internal extent");
            renderer.SetQuality(quality,scene).Value();render();renderer.WaitIdle().Value();Check(renderer.GetTelemetry().Value().Resident.TotalBytes==baseline&&target.Pixels(renderer)==reference,"restoring quality releases replacement targets and restores pixels");
            renderer.DetachTarget().Value();const auto restored=render();Check(restored.Presented&&target.Pixels(renderer)==reference,"detached borrowed target can be attached again without stale pixels");
            renderer.Shutdown().Value();renderer.Shutdown().Value();Check(!renderer.IsInitialized()&&!renderer.GetTelemetry().Value().Live,"shutdown is idempotent and leaves only historical telemetry");
        });}
    }catch(const std::exception& e){Check(false,e.what());}return Finish();
}
}
int main(){return GraphicsScope([]{return Run();});}
