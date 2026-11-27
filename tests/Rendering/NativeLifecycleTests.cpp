#include "TestHarness.h"
#include "GraphicsTestTarget.h"
#include <charconv>
#include <iomanip>
#include <thread>
using namespace test;
namespace {
struct Options {unsigned Cycles=10,Seconds=0;std::filesystem::path Output;};

void CheckResourcePoolRecovery(GraphicsTestTarget& target, const lv::ModelAsset& cube) {
    lv::EmbeddedRenderer renderer;
    lv::EmbeddedOptions options;
    options.Quality = lv::MakeQualityProfile(lv::QualityPreset::Low);
    options.Quality.Overrides.TargetFps = 0;
    renderer.Initialize(target.Context(), options).Value();
    lv::Scene scene;
    std::vector<lv::ObjectId> objects;
    for (int i = 0; i < 48; ++i) {
        objects.push_back(scene.AddModel(std::make_shared<lv::ModelAsset>(cube)));
    }
    Check(renderer.Render(scene, target.Target()).Value().SceneRendered,
        "more than 32 distinct models share shaders and pipelines with the default pools");
    for (int i = 0; i < 48; ++i) {
        objects.push_back(scene.AddModel(std::make_shared<lv::ModelAsset>(cube)));
    }
    const auto failed = renderer.Render(scene, target.Target());
    Check(!failed, "distinct models exhaust a native graphics resource pool");
    if (!failed) {
        const auto& error = failed.Error();
        Check(error.Code == lv::ErrorCode::InsufficientResource &&
            error.Message.find("Lifecycle") != std::string::npos &&
            error.Message.find("buffer pool exhausted") != std::string::npos &&
            error.Message.find("slots occupied") != std::string::npos,
            "pool exhaustion reports its resource name and slots occupied for the failed mesh");
    }
    for (const auto id : objects) { scene.RemoveObject(id); }
    const auto shared = std::make_shared<lv::ModelAsset>(cube);
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto object = scene.AddModel(shared);
        auto material = scene.GetMaterial(object, 0);
        material.BaseColor = {.3f, .5f, .8f, 1};
        scene.SetMaterial(object, 0, material);
    }
    const auto recovered = renderer.Render(scene, target.Target());
    Check(recovered && recovered.Value().Presented, "reusing a single model and overriding materials restores rendering");
    renderer.Shutdown().Value();
}

int Run(const Options& run) {
    try {
        const auto started=std::chrono::steady_clock::now();
        struct Sample {double Seconds;std::optional<std::uint64_t> Rss;std::uint64_t Logical;const char* Phase;};
        std::vector<Sample> samples;lv::SystemMonitor monitor;std::ofstream sampleFile;
        if(!run.Output.empty()){sampleFile.open(run.Output.string()+".csv");sampleFile<<"seconds,phase,logicalBytes,rssBytes\n"<<std::setprecision(12);if(!sampleFile){throw std::runtime_error("Cannot open lifecycle samples.");}}
        const auto memory=[&](const char* phase,std::uint64_t bytes) {
            const double now=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
            samples.push_back({now,monitor.Sample(now).ProcessResidentMemory.Value,bytes,phase});
            if(sampleFile.is_open()){const auto& sample=samples.back();sampleFile<<now<<','<<phase<<','<<bytes<<',';if(sample.Rss){sampleFile<<*sample.Rss;}sampleFile<<'\n';sampleFile.flush();if(!sampleFile){throw std::runtime_error("Cannot write lifecycle samples.");}}
        };
        GraphicsTestTarget target(96,64);lv::EmbeddedRenderer renderer;lv::Scene scene;
        auto model=std::make_shared<lv::ModelAsset>();model->Meshes.push_back({"Lifecycle",{{lv::MakeCube(),0,false}}});model->Parts.push_back({0});model->Materials.push_back({});model->ModelBounds={{-.5f,-.5f,-.5f},{.5f,.5f,.5f}};scene.AddModel(model);
        scene.MainCamera.Position={0,0,3};scene.MainCamera.Target={};scene.Sun.Intensity=scene.Sun.Ambient=0;scene.Ambient.Intensity=.2f;
        lv::PointLight point;point.Position={1,1,2};point.Range=8;point.Shadow.Enabled=true;scene.PointLights={point};scene.LocalShadowResolution=128;
        lv::RectLight rect;rect.Position={0,2,2};rect.Direction={0,-1,-1};scene.RectLights={rect};
        auto quality=lv::MakeQualityProfile(lv::QualityPreset::Low);quality.Overrides.Lighting=lv::LightingMode::Advanced;quality.Overrides.Shadow=lv::ShadowMode::Basic;quality.Overrides.TargetFps=0;quality.Overrides.OnDemand=lv::OnDemandMode::On;
        lv::EmbeddedOptions options;options.Quality=quality;lv::EmbeddedFrameOptions frame;frame.ClearColor={0,0,0,1};
        std::uint64_t baseline=0;std::vector<std::uint8_t> reference;
        for(unsigned cycle=0;cycle<run.Cycles;++cycle){GraphicsScope([&]{
            renderer.GetUi().Clear().Value();renderer.Initialize(target.Context(),options).Value();
            const auto render=[&]{return renderer.Render(scene,target.Target(),frame).Value();};render();renderer.WaitIdle().Value();
            const auto pixels=target.Pixels(renderer);const auto bytes=renderer.GetTelemetry().Value().Resident.TotalBytes;
            if(!cycle){baseline=bytes;reference=pixels;}
            Check(bytes==baseline&&pixels==reference,"reinitialized session returns to the same logical resource cost and pixels");
            Check(!render().SceneRendered,"unchanged session image stays cached");
            auto scaled=quality;scaled.Overrides.ResolutionScale=.5f;renderer.SetQuality(scaled,scene).Value();Check(render().RenderWidth==48,"quality resize uses the reduced internal extent");
            renderer.SetQuality(quality,scene).Value();render();renderer.WaitIdle().Value();Check(renderer.GetTelemetry().Value().Resident.TotalBytes==baseline&&target.Pixels(renderer)==reference,"restoring quality releases replacement targets and restores pixels");
            renderer.DetachTarget().Value();const auto restored=render();Check(restored.Presented&&target.Pixels(renderer)==reference,"detached borrowed target can be attached again without stale pixels");
            if(!run.Output.empty()) {
                renderer.WaitIdle().Value();renderer.DetachTarget().Value();target.Resize(128,80);
                const auto resized=render();Check(resized.Presented&&resized.RenderWidth==128&&target.Pixels(renderer).size()==128*80*4,"active session accepts a resized host target");
                renderer.WaitIdle().Value();renderer.DetachTarget().Value();target.Resize(96,64);render();
                Check(target.Pixels(renderer)==reference&&renderer.GetTelemetry().Value().Resident.TotalBytes==baseline,"host target resize restores pixels and logical allocations");
            }
            renderer.Shutdown().Value();renderer.Shutdown().Value();Check(!renderer.IsInitialized()&&!renderer.GetTelemetry().Value().Live,"shutdown is idempotent and leaves only historical telemetry");
            if(!run.Output.empty()) {memory("after-shutdown",0);}
        });}
        if(run.Seconds) {
            renderer.Initialize(target.Context(),options).Value();
            auto& ui=renderer.GetUi();lv::UiElement label;label.Text="Stability";label.Layout={0,0,90,30};const auto id=ui.Add(label).Value();ui.ClearHistory().Value();
            const auto soak=std::chrono::steady_clock::now();unsigned frames=0;double nextSample=0;std::uint64_t soakBytes=0;
            while(std::chrono::steady_clock::now()-soak<std::chrono::seconds(run.Seconds)) {
                const auto tick=std::chrono::steady_clock::now();
                GraphicsScope([&] {
                    label.Text="Frame "+std::to_string(frames++);ui.Set(id,label).Value();ui.ClearHistory().Value();
                    renderer.Render(scene,target.Target(),frame).Value();
                    const double elapsed=std::chrono::duration<double>(tick-soak).count();
                    if(elapsed>=nextSample) {
                        const auto bytes=renderer.GetTelemetry().Value().Resident.TotalBytes;
                        if(!soakBytes) {soakBytes=bytes;}Check(bytes==soakBytes,"long-running dynamic UI keeps bounded logical GPU allocations");
                        memory("soak",bytes);nextSample=elapsed+1;
                    }
                });
                std::this_thread::sleep_until(tick+std::chrono::milliseconds(16));
            }
            renderer.WaitIdle().Value();renderer.Shutdown().Value();memory("soak-shutdown",0);
        }
        if(!run.Output.empty()) {
            std::ofstream out(run.Output);out<<std::setprecision(12)<<"{\"format\":\"MinvexLifecycleProbe\",\"cycles\":"<<run.Cycles<<",\"soakSeconds\":"<<run.Seconds<<",\"elapsedSeconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"failures\":"<<Failures<<",\"samples\":[";
            for(std::size_t i=0;i<samples.size();++i) {const auto& s=samples[i];if(i){out<<',';}out<<"{\"seconds\":"<<s.Seconds<<",\"phase\":\""<<s.Phase<<"\",\"logicalBytes\":"<<s.Logical<<",\"rssBytes\":";if(s.Rss){out<<*s.Rss;}else{out<<"null";}out<<'}';}
            out<<"]}\n";out.close();if(!out){throw std::runtime_error("Cannot save lifecycle report.");}
        }
        CheckResourcePoolRecovery(target, *model);
    }catch(const std::exception& e){Check(false,e.what());}return Finish();
}
}
int main(int argc,char** argv){return GraphicsScope([&] {
    Options options;
    try {
        for(int i=1;i<argc;++i) {
            const std::string_view key=argv[i];if(i+1==argc){throw std::invalid_argument("Missing lifecycle option value.");}
            const std::string_view value=argv[++i];
            if(key=="--output") {options.Output=value;continue;}
            unsigned number=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),number);
            if(error!=std::errc{}||end!=value.data()+value.size()){throw std::invalid_argument("Invalid lifecycle count.");}
            if(key=="--cycles"&&number>=1&&number<=1000){options.Cycles=number;}
            else if(key=="--soak-seconds"&&number<=7200){options.Seconds=number;}
            else {throw std::invalid_argument("Expected --cycles 1..1000, --soak-seconds 0..7200, or --output path.");}
        }
        return Run(options);
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 2;}
});}
