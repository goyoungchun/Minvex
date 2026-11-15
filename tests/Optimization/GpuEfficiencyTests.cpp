#include "TestHarness.h"
#include "OptimizationFixture.h"
#include "GraphicsTestTarget.h"
#include <cstring>
#include <iomanip>
using namespace test;
namespace {
struct Harness {
    int Width,Height;
    GraphicsTestTarget Target;
    lv::EmbeddedRenderer Renderer;lv::Scene Scene;
    Harness(int width=320,int height=240):Width(width),Height(height),Target(width,height) {
        lv::EmbeddedOptions options;options.Quality=lv::MakeQualityProfile(lv::QualityPreset::Low);options.Quality.Overrides.TargetFps=0;options.Quality.Overrides.OnDemand=lv::OnDemandMode::Off;options.Quality.Overrides.FrustumCulling=false;
        Renderer.Initialize(Target.Context(),options).Value();Scene.MainCamera.Position={0,0,3};Scene.MainCamera.Target={};Scene.MainCamera.Projection=lv::CameraProjection::Orthographic;Scene.MainCamera.OrthographicHeight=2.5f;
    }
    void Render() {GraphicsScope([&]{lv::EmbeddedFrameOptions frame;frame.ClearColor={0,0,0,1};frame.ForceRedraw=true;Renderer.Render(Scene,Target.Target(),frame).Value();});}
    void Clear() {const auto ids=Scene.ObjectIds();for(auto id:ids){if(Scene.Contains(id)){Scene.RemoveObject(id);}}}
    std::vector<std::uint8_t> Pixels() {return Target.Pixels(Renderer);}
    lv::DebugResource Resource(const char* name) {const auto info=Renderer.GetRenderDebugInfo().Value();const auto it=std::find_if(info.Resources.begin(),info.Resources.end(),[&](const auto& r){return r.Name==name;});if(it==info.Resources.end()){throw std::runtime_error(std::string("Missing resource: ")+name);}return *it;}
    ~Harness(){if(Renderer.IsInitialized()){Renderer.Shutdown().Value();}}
};
std::shared_ptr<lv::ModelAsset> Model(lv::MeshData mesh,std::vector<lv::MeshLod> lods={}) {
    auto model=std::make_shared<lv::ModelAsset>();lv::MeshPrimitive p;p.Geometry=std::move(mesh);p.MaterialIndex=0;p.Lods=std::move(lods);model->Meshes.push_back({"Cache fixture",{std::move(p)}});model->Parts.push_back({0});model->Materials.push_back({});model->Materials[0].Unlit=true;model->ModelBounds={{-1,-1,-1},{1,1,1}};return model;
}
lv::MeshData DenseGrid(unsigned n) {
    lv::MeshData mesh;
    for(unsigned y=0;y<=n;++y){for(unsigned x=0;x<=n;++x){lv::Vertex v{};v.Position={2.0f*x/n-1,2.0f*y/n-1,0};v.Normal={0,0,1};v.Color={1,1,1,1};mesh.Vertices.push_back(v);}}
    std::vector<std::array<std::uint32_t,3>> triangles;
    for(unsigned y=0;y<n;++y){for(unsigned x=0;x<n;++x){const auto a=y*(n+1)+x;triangles.push_back({a,a+1,a+n+2});triangles.push_back({a,a+n+2,a+n+1});}}
    std::mt19937 random(19);std::shuffle(triangles.begin(),triangles.end(),random);for(auto t:triangles){mesh.Indices.insert(mesh.Indices.end(),t.begin(),t.end());}return mesh;
}
void Correctness() {
    Harness h;
    for (std::size_t count:{std::size_t(65536),std::size_t(65537)}) {
        lv::MeshData mesh;mesh.Vertices.resize(count);for(auto& v:mesh.Vertices){v.Normal={0,0,1};v.Color={1,1,1,1};}
        mesh.Vertices[count-3].Position={-.8f,-.8f,0};mesh.Vertices[count-2].Position={.8f,-.8f,0};mesh.Vertices[count-1].Position={0,.8f,0};mesh.Indices={std::uint32_t(count-3),std::uint32_t(count-2),std::uint32_t(count-1)};
        h.Clear();h.Scene.AddModel(Model(std::move(mesh)));h.Render();const auto pixels=h.Pixels();
        Check(pixels[(120*320+160)*4]>200,"16/32-bit index boundary produces real triangle pixels");
        Check(h.Resource("Mesh triangle indices").Bytes==3*(count<=65536 ? 2:4),"actual GPU index buffer matches width selection at boundary");
    }
    const auto grid=OptimizationGrid(true);const auto lods=lv::GenerateMeshLods(grid);
    h.Clear();h.Scene.AddModel(Model(grid,lods));lv::DebugOptions debug;debug.View=lv::DebugView::Wireframe;h.Renderer.SetDebugOptions(debug).Value();h.Render();const auto withLods=h.Pixels();
    h.Clear();h.Scene.AddModel(Model(grid));h.Render();
    Check(withLods==h.Pixels(),"wireframe uses uploaded vertex remap with coarsest-first LOD vertex order");
    h.Renderer.SetDebugOptions({}).Value();h.Clear();const auto cube=Model(lv::MakeCube());h.Scene.AddModel(cube);lv::Transform transform;transform.Position={1,0,0};const auto moving=h.Scene.AddModel(cube,transform);
    h.Render();const auto first=h.Resource("Static instance transforms");const auto initial=h.Pixels();h.Render();
    Check(h.Resource("Static instance transforms").Id==first.Id&&h.Pixels()==initial,"unchanged static instance buffer and pixels are reused");
    transform.Position={-1,0,0};h.Scene.SetTransform(moving,transform);h.Render();
    Check(h.Resource("Static instance transforms").Id!=first.Id&&h.Pixels()!=initial,"changed instance transform replaces its GPU data and pixels");
    auto& ui=h.Renderer.GetUi();lv::UiElement label;label.Text="Streaming";label.Layout={10,10,180,50};const auto id=ui.Add(label).Value();h.Render();
    const auto bitmap=h.Resource("UI bitmap").Id,upload=h.Resource("UI upload staging").Id;
    label.Text="Updated";ui.Set(id,label).Value();h.Render();
    Check(h.Resource("UI bitmap").Id==bitmap&&h.Resource("UI upload staging").Id==upload,"UI updates reuse persistent texture and staging buffer");
    h.Renderer.Shutdown().Value();
    Harness odd(317,239);lv::UiElement panel;panel.Kind=lv::UiKind::Panel;panel.Layout={0,0,317,239};panel.Normal.Fill.fill({1,0,0,1});odd.Renderer.GetUi().Add(panel).Value();odd.Render();
    const auto oddPixels=odd.Pixels();
    Check(oddPixels[(238*317+316)*4]>200&&oddPixels[(238*317+316)*4+1]<10,"padded odd-width UI upload preserves the final row and column");
}
void Benchmark(const std::string& mode,const char* output) {
    Harness h(1280,720);auto quality=lv::MakeQualityProfile(lv::QualityPreset::Low);quality.Overrides.TargetFps=0;quality.Overrides.OnDemand=lv::OnDemandMode::Off;quality.Overrides.FrustumCulling=false;quality.Overrides.Instancing=mode=="instancing";quality.Overrides.Lod=true;
    auto mesh=DenseGrid(mode=="overdraw" ? 64:128);std::vector<lv::MeshLod> lods;
    if(mode=="overdraw") {auto plane=mesh;mesh={};for(unsigned layer=0;layer<12;++layer){const auto base=static_cast<std::uint32_t>(mesh.Vertices.size());for(auto vertex:plane.Vertices){vertex.Position[2]=layer*.04f;mesh.Vertices.push_back(vertex);}for(auto index:plane.Indices){mesh.Indices.push_back(base+index);}}quality.Overrides.Lod=false;quality.Overrides.Lighting=lv::LightingMode::Advanced;quality.Overrides.Pbr=true;}
    else if(mode=="instancing") {mesh=lv::MakeCube();}
    else {lods=lv::GenerateMeshLods(mesh);if(mode=="far-lod"){h.Scene.MainCamera.OrthographicHeight=80;}}
    const auto model=Model(std::move(mesh),std::move(lods));if(mode=="overdraw"){model->Materials[0].Unlit=false;model->Materials[0].Shading=lv::ShadingModel::Pbr;}
    const int objects=mode=="instancing" ? 1000:1;for(int i=0;i<objects;++i){lv::Transform t;if(objects>1){t.Position={float(i%40)-20,float(i/40)-12,0};}h.Scene.AddModel(model,t);}if(objects>1){h.Scene.MainCamera.OrthographicHeight=30;}
    h.Renderer.SetQuality(quality,h.Scene).Value();const auto warmStart=std::chrono::steady_clock::now();unsigned warmup=0;do{h.Render();++warmup;}while(warmup<120||std::chrono::steady_clock::now()-warmStart<std::chrono::milliseconds(500));h.Renderer.WaitIdle().Value();
    const unsigned frames=mode=="instancing" ? 1200:6000;
    const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<frames;++i){h.Render();}h.Renderer.WaitIdle().Value();
    const double completed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();const auto telemetry=h.Renderer.GetTelemetry().Value();
    std::ofstream file(output);file<<std::setprecision(12)<<"{\"format\":\"MinvexGpuCacheProbe\",\"version\":2,\"mode\":\""<<mode<<"\",\"width\":1280,\"height\":720,\"warmup\":"<<warmup<<",\"frames\":"<<frames<<",\"objects\":"<<objects<<",\"completedBatchMs\":"<<completed<<",\"completedMeanMs\":"<<completed/frames<<",\"logicalBuffers\":"<<telemetry.Resident.BufferBytes<<",\"instanceBufferBytes\":"<<telemetry.Optimization.InstanceBufferBytes<<",\"submittedTriangles\":"<<telemetry.Optimization.SubmittedTriangles<<",\"gpuSamples\":"<<telemetry.Frames.GpuSampleCount<<",\"gpuMeanMs\":";
    if(telemetry.Frames.AverageGpuMs.Value){file<<*telemetry.Frames.AverageGpuMs.Value;}else{file<<"null";}file<<",\"cpuMeanMs\":";if(telemetry.Frames.AverageCpuMs.Value){file<<*telemetry.Frames.AverageCpuMs.Value;}else{file<<"null";}file<<"}\n";file.close();if(!file){throw std::runtime_error("Cannot save GPU benchmark");}
    std::printf("%s: completed %.3f ms/frame\n",mode.c_str(),completed/frames);
}
}
int main(int argc,char** argv) {return GraphicsScope([&]{try {if(argc==3){Benchmark(argv[1],argv[2]);}else{Correctness();}}catch(const std::exception& e){Check(false,e.what());}return Finish();});}
