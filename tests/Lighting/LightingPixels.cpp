#include "TestHarness.h"
#include "GraphicsTestTarget.h"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <iomanip>
using namespace test;
namespace {
struct Host {
    int W,H;
    GraphicsTestTarget Target;
    lv::EmbeddedRenderer Renderer;lv::Scene Scene;
    lv::QualityProfile Quality=lv::MakeQualityProfile(lv::QualityPreset::Low);
    Host(int width=320,int height=240):W(width),H(height),Target(width,height){
        Quality.Overrides.Lighting=lv::LightingMode::Advanced;Quality.Overrides.TargetFps=0;Quality.Overrides.OnDemand=lv::OnDemandMode::Off;Quality.Overrides.Instancing=false;Quality.Overrides.Shadow=lv::ShadowMode::Basic;
        lv::EmbeddedOptions o;o.Quality=Quality;Renderer.Initialize(Target.Context(),o).Value();Scene.Sun.Intensity=Scene.Sun.Ambient=0;Scene.MainCamera.Position={0,0,6};Scene.MainCamera.Target={};Scene.MainCamera.Projection=lv::CameraProjection::Orthographic;Scene.MainCamera.OrthographicHeight=4;Scene.LocalShadowResolution=256;
    }
    auto Render(bool force=true){return GraphicsScope([&]{lv::EmbeddedFrameOptions f;f.ClearColor={0,0,0,1};f.ForceRedraw=force;return Renderer.Render(Scene,Target.Target(),f).Value();});}
    auto Pixels(){return Target.Pixels(Renderer);}
    std::uint64_t ShadowCalls(){const auto info=Renderer.GetRenderDebugInfo().Value();std::uint64_t calls=0;for(const auto& p:info.Passes){if(p.Name=="Sun shadow"||p.Name=="Local shadows"){calls+=p.DrawCalls;}}return calls;}
    lv::DebugResource Resource(const char* name){const auto info=Renderer.GetRenderDebugInfo().Value();for(const auto& r:info.Resources){if(r.Name==name){return r;}}throw std::runtime_error(std::string("Missing ")+name);}
    ~Host(){if(Renderer.IsInitialized()){Renderer.Shutdown().Value();}}
};
auto Model(lv::MeshData mesh){auto m=std::make_shared<lv::ModelAsset>();lv::MeshPrimitive p;p.Geometry=std::move(mesh);m->Meshes.push_back({"Lighting",{std::move(p)}});m->Parts.push_back({0});m->Materials.push_back({});m->Materials[0].DoubleSided=true;m->ModelBounds={{-3,-3,-3},{3,3,3}};return m;}
auto Floor(float half=3){lv::MeshData m;for(auto p:std::array<std::array<float,3>,4>{{{-half,-half,0},{half,-half,0},{half,half,0},{-half,half,0}}}){lv::Vertex v;v.Position=p;v.Normal={0,0,1};v.Color={1,1,1,1};m.Vertices.push_back(v);}m.Indices={0,1,2,0,2,3};return Model(std::move(m));}
std::uint64_t Energy(const std::vector<std::uint8_t>& p){std::uint64_t n=0;for(std::size_t i=0;i<p.size();i+=4){n+=p[i]+p[i+1]+p[i+2];}return n;}
std::size_t Changed(const auto& a,const auto& b){std::size_t n=0;for(std::size_t i=0;i<a.size();i+=4){n+=std::abs(int(a[i])-int(b[i]))+std::abs(int(a[i+1])-int(b[i+1]))+std::abs(int(a[i+2])-int(b[i+2]))>20;}return n;}
#include "LightingCoverage.h"
void PanoramaAndTransparentMeshes() {
    Host h(96,64);
    constexpr auto center = 4 * (32 * 96 + 48);
    auto sky = std::make_shared<lv::ImageAsset>();
    sky->Width = 64;
    sky->Height = 32;
    for (int y = 0; y < sky->Height; ++y) {
        for (int x = 0; x < sky->Width; ++x) {
            const bool red = x >= 24 && x < 40;
            sky->Rgba.insert(sky->Rgba.end(), {static_cast<std::uint8_t>(red ? 128 : 0), 0,
                static_cast<std::uint8_t>(red ? 0 : 255), 255});
        }
    }
    h.Quality.Overrides.OnDemand = lv::OnDemandMode::On;
    h.Renderer.SetQuality(h.Quality, h.Scene).Value();
    h.Scene.SetEnvironmentPanorama({sky});
    Check(h.Render(false).SceneRendered, "setting the panorama invalidates an on-demand frame");
    const auto original = h.Pixels();
    Check(std::abs(int(original[center]) - 55) <= 4 && original[center+2] < 5,
        "the panorama center decodes sRGB into the linear host target");
    const auto telemetry = h.Renderer.GetTelemetry().Value();
    const auto estimate = lv::EstimateQualityResources(h.Quality, telemetry.Quality.Capabilities, h.Scene, h.W, h.H).Value();
    Check(estimate.Cost.TextureBytes == telemetry.Resident.TextureBytes && telemetry.Resident.TextureBytes == sky->Rgba.size(),
        "panorama residency matches the texture estimate");
    auto& camera = h.Scene.MainCamera;
    camera.Position = {100, 200, 300};
    camera.Target = {100, 200, 299};
    camera.Projection = lv::CameraProjection::Perspective;
    camera.FarPlane = 5;
    h.Render(false);
    Check(h.Pixels() == original, "camera translation, clipping and projection mode do not move the sky");
    camera.Target = {100, 200, 301};
    h.Render(false);
    Check(h.Pixels()[center+2] > 240, "camera rotation turns toward a different part of the panorama");
    h.Scene.SetEnvironmentPanorama({sky, 3.14159265359f, .5f});
    Check(h.Render(false).SceneRendered && std::abs(int(h.Pixels()[center]) - 28) <= 4,
        "sky rotation and linear intensity update an on-demand frame");
    auto high = h.Quality;
    high.Overrides.PostProcessing = true;
    high.Overrides.MsaaSamples = 4;
    high.AllowFallback = true;
    h.Renderer.SetQuality(high, h.Scene).Value();
    h.Render();
    Check(h.Pixels()[center] > 20, "the panorama survives HDR and MSAA changes");
    h.Scene.SetEnvironmentPanorama({});
    h.Render();
    Check(h.Pixels()[center] == 0, "clearing the sky restores the frame clear color");

    camera = {};
    camera.Position = {0,0,6};
    camera.Projection = lv::CameraProjection::Orthographic;
    camera.OrthographicHeight = 4;
    h.Quality.Overrides.Instancing = true;
    h.Renderer.SetQuality(h.Quality, h.Scene).Value();
    const auto panel = [](std::array<float,4> color, bool transparent) {
        auto model = Floor();
        auto& material = model->Materials.front();
        material.Unlit = true;
        material.AlphaBlend = transparent;
        material.BaseColor = color;
        return model;
    };
    lv::Transform placement;
    placement.Position.Z = 1;
    const auto nearPanel = h.Scene.AddModel(panel({1,0,0,.5f}, true), placement);
    h.Scene.AddModel(panel({0,1,0,.5f}, true));
    h.Render();
    const auto mixed = h.Pixels();
    Check(std::abs(int(mixed[center]) - 128) <= 4 && std::abs(int(mixed[center+1]) - 64) <= 4 &&
          mixed[center+2] < 5 && h.ShadowCalls() == 0,
        "alpha blended geometry sorts by depth and skips the shadow pass");
    placement.Position.Z = .5f;
    h.Scene.AddModel(panel({0,0,1,1}, false), placement);
    h.Render();
    const auto occluded = h.Pixels();
    Check(std::abs(int(occluded[center]) - 128) <= 4 && occluded[center+1] < 5 &&
          std::abs(int(occluded[center+2]) - 128) <= 4 && h.ShadowCalls() == 1,
        "opaque depth occludes a far blended panel and leaves the nearer panel visible");
    h.Renderer.SetQuality(high, h.Scene).Value();
    h.Render();
    const auto highPixels = h.Pixels();
    Check(highPixels[center] > 75 && highPixels[center+2] > 75, "alpha blended meshes survive a quality change");
    h.Renderer.SetQuality(h.Quality, h.Scene).Value();
    h.Scene.AddModel(h.Scene.GetObject(nearPanel).Model, placement);
    h.Render();
    Check(h.Renderer.GetTelemetry().Value().Optimization.InstancedObjects == 0, "shared transparent models retain individual draws");
}
void CasterVariants(Host& h) {
    for(auto id:h.Scene.ObjectIds()){if(h.Scene.Contains(id)){h.Scene.RemoveObject(id);}}
    h.Scene.RectLights.clear();h.Scene.SpotLights.clear();h.Scene.Ambient.Intensity=0;
    lv::PointLight light;light.Position={-1,1,3};light.Intensity=8;light.Range=10;light.Shadow.Enabled=true;h.Scene.PointLights={light};
    h.Quality.Overrides.OnDemand=lv::OnDemandMode::Off;h.Quality.Overrides.Instancing=true;h.Renderer.SetQuality(h.Quality,h.Scene).Value();h.Scene.AddModel(Floor());
    auto cube=Model(lv::MakeCube());lv::Transform t;t.Position={0,0,1};const auto a=h.Scene.AddModel(cube,t);t.Position.X=1;const auto b=h.Scene.AddModel(cube,t);h.Render();const auto opaque=h.Pixels();
    auto mask=h.Scene.GetMaterial(a,0);mask.AlphaCutoff=.5f;mask.BaseColor[3]=0;h.Scene.SetMaterial(a,0,mask);h.Scene.SetMaterial(b,0,mask);h.Render();
    Check(h.ShadowCalls()>0&&Energy(h.Pixels())>Energy(opaque),"alpha-mask edits refresh and remove point-shadow occlusion");
    h.Scene.ResetMaterial(a,0);h.Scene.ResetMaterial(b,0);h.Render();const auto instanced=h.Pixels();h.Quality.Overrides.Instancing=false;h.Renderer.SetQuality(h.Quality,h.Scene).Value();h.Render();
    Check(Changed(instanced,h.Pixels())<10,"instanced local-shadow projection matches individual draws");h.Scene.RemoveObject(a);h.Scene.RemoveObject(b);
    lv::AssetRegistry assets;auto rig=std::make_shared<lv::ModelAsset>(*assets.LoadModel(Fixture("MorphRibbon.gltf")));rig->Parts.resize(1);rig->Materials[0].Unlit=false;rig->Materials[0].DoubleSided=true;
    t.Position={0,0,1};const auto id=h.Scene.AddModel(rig,t);const std::array<float,2> weights{.8f,.2f};auto& player=h.Scene.GetAnimation(id);player.SetMorphWeights(3,weights);player.Play(0);player.Seek(.5);player.Pause();h.Render();const auto gpu=h.Pixels();h.Render();Check(h.ShadowCalls()==0,"paused Morph and Skin point shadows remain cached");
    auto reference=std::make_shared<lv::ModelAsset>(*rig);reference->Meshes[0].Primitives[0].Geometry=lv::EvaluateMorph(rig->Meshes[0].Primitives[0],weights);reference->Meshes[0].Primitives[0].MorphTargets.clear();reference->Meshes[0].MorphWeights.clear();for(auto& node:reference->Nodes){node.MorphWeights.clear();}for(auto& clip:reference->Animations){clip.MorphTracks.clear();}
    h.Scene.SetModel(id,reference);auto& expected=h.Scene.GetAnimation(id);expected.Play(0);expected.Seek(.5);expected.Pause();h.Render();Check(Changed(gpu,h.Pixels())<20,"Morph-before-Skin point shadow pixels match CPU-deformed geometry");
    h.Scene.PointLights[0].Shadow.DepthBias=20;h.Render();Check(Changed(gpu,h.Pixels())>30,"Morph and Skin geometry actually occludes the point-lit receiver");
    h.Scene.SetModel(id,rig);auto& animated=h.Scene.GetAnimation(id);animated.Play(0);animated.Seek(.5);animated.Pause();h.Scene.PointLights[0].Shadow.DepthBias=.005f;h.Render();const auto before=h.Pixels();animated.SetMorphWeights(3,std::array<float,2>{0,0});h.Render();Check(h.ShadowCalls()>0&&Changed(before,h.Pixels())>20,"Morph edits invalidate cached local shadows and change pixels");
}
}
void Benchmark(const char* output){
    Host h(1280,720);h.Scene.LocalShadowResolution=512;h.Scene.MainCamera.OrthographicHeight=12;h.Scene.MainCamera.Position={0,0,10};h.Scene.Ambient.Intensity=.05f;auto floor=Floor(),cube=Model(lv::MakeCube());for(auto m:{floor,cube}){m->Materials[0].Shading=lv::ShadingModel::Pbr;m->Materials[0].MetallicFactor=.2f;m->Materials[0].RoughnessFactor=.6f;}h.Scene.AddModel(floor);for(int i=0;i<100;++i){lv::Transform t;t.Position={float(i%10)-4.5f,float(i/10)-4.5f,.75f};h.Scene.AddModel(cube,t);}
    for(int i=0;i<4;++i){lv::PointLight p;p.Position={i&1 ? 3.0f:-3.0f,i&2 ? 3.0f:-3.0f,4};p.Intensity=8;p.Range=12;h.Scene.PointLights.push_back(p);lv::RectLight r;r.Position={p.Position.X,p.Position.Y,5};r.Direction={0,0,-1};r.Up={0,1,0};r.Width=2;r.Height=1.5f;r.Range=12;h.Scene.RectLights.push_back(r);}
    h.Quality.Overrides.Pbr=true;h.Quality.Overrides.Instancing=true;h.Renderer.SetQuality(h.Quality,h.Scene).Value();std::ofstream out(output);out<<std::setprecision(12)<<"{\"format\":\"MinvexLightingBenchmark\",\"version\":1,\"width\":1280,\"height\":720,\"cubeInstances\":100,\"pointLights\":4,\"rectLights\":4,\"shadowFaceSize\":512,\"frames\":600,\"scope\":\"completed offscreen batch includes WaitIdle, not displayed FPS\",\"cases\":[";
    for(int mode=0;mode<4;++mode){for(auto& p:h.Scene.PointLights){p.Shadow.Enabled=mode>0;}unsigned frame=0;const auto render=[&]{if(mode>=2){const int lights=mode==2 ? 1:4;for(int i=0;i<lights;++i){h.Scene.PointLights[i].Position.X=(i&1 ? 3.0f:-3.0f)+std::sin(float(frame)*.02f)*.5f;}}++frame;h.Render();};const auto warm=std::chrono::steady_clock::now();do{render();}while(frame<120||std::chrono::steady_clock::now()-warm<std::chrono::seconds(1));h.Renderer.WaitIdle().Value();const auto start=std::chrono::steady_clock::now();for(int f=0;f<600;++f){render();}h.Renderer.WaitIdle().Value();const double mean=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/600;const auto calls=h.ShadowCalls();const auto telemetry=h.Renderer.GetTelemetry().Value();const char* names[]={"no-local-shadows","static-cached","one-moving-point","four-moving-points"};if(mode){out<<',';}out<<"{\"name\":\""<<names[mode]<<"\",\"completedMeanMs\":"<<mean<<",\"lastFrameShadowDrawCalls\":"<<double(calls)<<",\"logicalBytes\":"<<telemetry.Resident.TotalBytes<<'}';std::printf("%s: %.3f ms, %.1f shadow calls/frame\n",names[mode],mean,double(calls));}
    out<<"]}\n";out.close();if(!out){throw std::runtime_error("Cannot save lighting benchmark");}
}
int RunLightingPixels(int argc,char** argv){try{if(argc==2&&std::string_view(argv[1])=="--write-references"){PcssAndHdr(true);return Finish();}if(argc==3&&std::string_view(argv[1])=="--shader-variants"){ShaderVariants(argv[2]);return Finish();}if(argc==2){Benchmark(argv[1]);return 0;}{
    Host h;const auto floor=h.Scene.AddModel(Floor());h.Render();const auto dark=h.Pixels();h.Scene.Ambient={{1,0,0},.4f};h.Render();const auto ambient=h.Pixels();Check(ambient[(120*320+160)*4]>100&&ambient[(120*320+160)*4+1]<5,"independent colored ambient produces real pixels with the Sun disabled");h.Scene.Ambient.Intensity=0;
    lv::PointLight point;point.Position={-1,1,3};point.Intensity=8;point.Range=10;h.Scene.PointLights={point};lv::Transform t;t.Position={0,0,1};const auto cube=h.Scene.AddModel(Model(lv::MakeCube()),t);h.Render();const auto lit=h.Pixels();h.Scene.PointLights[0].Shadow.Enabled=true;h.Render();const auto shadow=h.Pixels();Check(Changed(lit,shadow)>100&&Energy(shadow)<Energy(lit),"point shadows occlude the receiver");const float q=.70710678118f;
    for(const auto rotation:std::array<lv::Quaternion,6>{{{0,0,0,1},{0,q,0,q},{0,-q,0,q},{0,1,0,0},{q,0,0,q},{-q,0,0,q}}}) {
        const auto matrix=lv::Mat4::Rotation(rotation);lv::Transform plane;plane.Rotation=rotation;h.Scene.SetTransform(floor,plane);lv::Transform box;box.Rotation=rotation;box.Position=matrix.TransformPoint({0,0,1});h.Scene.SetTransform(cube,box);h.Scene.MainCamera.Position=matrix.TransformPoint({0,0,6});h.Scene.MainCamera.Up=matrix.TransformPoint({0,1,0});auto& light=h.Scene.PointLights[0];light.Position=matrix.TransformPoint(point.Position);light.Shadow.DepthBias=20;h.Render();const auto reference=h.Pixels();light.Shadow.DepthBias=.005f;h.Render();const auto occluded=h.Pixels();Check(Changed(reference,occluded)>100&&Energy(occluded)<Energy(reference),"point shadows cover all six world directions");
    }
    h.Scene.SetTransform(floor,{});h.Scene.SetTransform(cube,t);h.Scene.MainCamera.Position={0,0,6};h.Scene.MainCamera.Up={0,1,0};h.Scene.PointLights[0].Position=point.Position;h.Render();
    const auto atlas=h.Resource("Local shadow atlas");h.Render();Check(h.ShadowCalls()==0&&h.Pixels()==shadow,"unchanged Sun and point shadow maps are reused");
    const auto validQuality=h.Renderer.GetQuality().Value();
    h.Scene.PointLights[0].Range=-1;const auto badLight=h.Renderer.SetQuality(h.Quality,h.Scene);h.Scene.PointLights[0].Range=point.Range;
    Check(!badLight&&badLight.Error().Code==lv::ErrorCode::InvalidArgument,"quality application reports invalid lighting as an input error");
    h.Scene.LocalShadowResolution=777;const auto badResolution=h.Renderer.SetQuality(h.Quality,h.Scene);h.Scene.LocalShadowResolution=256;
    Check(!badResolution&&badResolution.Error().Code==lv::ErrorCode::InvalidArgument,"quality application rejects invalid scene shadow resolution as an input error");
    const auto afterFailure=h.Renderer.GetQuality().Value();h.Render();
    Check(afterFailure.Requested==validQuality.Requested&&afterFailure.Effective==validQuality.Effective&&h.Resource("Local shadow atlas").Id==atlas.Id&&h.Pixels()==shadow,"invalid lighting leaves active quality, shadow allocation and pixels intact");
    h.Scene.PointLights[0].Color={.3f,1,.3f};h.Render();Check(h.ShadowCalls()==0&&Changed(shadow,h.Pixels())>100,"changing light color updates lighting without rebuilding shadows");h.Scene.MainCamera.Position.X=.2f;h.Render();Check(h.ShadowCalls()==0,"camera movement does not rebuild shadow maps");h.Scene.MainCamera.Position.X=0;
    const auto sunDirection = h.Scene.Sun.Direction;
    h.Scene.Sun.Direction = {-1,-1,-1};
    h.Render();
    const auto directional = h.Renderer.GetRenderDebugInfo().Value();
    const auto sunPass = std::ranges::find(directional.Passes, "Sun shadow", &lv::DebugPass::Name);
    const auto localPass = std::ranges::find(directional.Passes, "Local shadows", &lv::DebugPass::Name);
    Check(sunPass != directional.Passes.end() && sunPass->DrawCalls > 0 &&
          localPass != directional.Passes.end() && localPass->DrawCalls == 0,
          "changing the Sun direction rebuilds its projection without redrawing local shadows");
    h.Scene.Sun.Direction = sunDirection;
    h.Render();
    t.Position.X=.5f;h.Scene.SetTransform(cube,t);h.Render();Check(h.ShadowCalls()>0&&h.Resource("Local shadow atlas").Id==atlas.Id,"moving a caster refreshes existing shadow resources");
    h.Quality.Overrides.Anisotropy=2;h.Renderer.SetQuality(h.Quality,h.Scene).Value();h.Render();Check(h.ShadowCalls()>0&&h.Resource("Local shadow atlas").Id==atlas.Id,"alpha sampling quality changes refresh shadow contents while preserving atlas allocation");
    const auto originalPoint=h.Scene.PointLights[0];auto secondPoint=originalPoint;secondPoint.Position={1,1,3};h.Scene.PointLights.push_back(secondPoint);h.Render();h.Render();Check(h.ShadowCalls()==0,"two unchanged local shadow maps stay cached");h.Scene.PointLights[0].Position.X-=.1f;h.Render();const auto partial=h.ShadowCalls();h.Scene.PointLights[0].Position.X-=.1f;h.Scene.PointLights[1].Position.X+=.1f;h.Render();Check(partial>0&&h.ShadowCalls()>partial,"moving one light updates fewer atlas tiles than moving both");h.Scene.PointLights={originalPoint};
    h.Scene.PointLights.clear();lv::SpotLight spot;spot.Position={-1,1,3};spot.Direction={1,-1,-3};spot.Intensity=12;spot.InnerConeRadians=.4f;spot.OuterConeRadians=.7f;h.Scene.SpotLights={spot};h.Render();const auto spotLit=h.Pixels();h.Scene.SpotLights[0].Shadow.Enabled=true;h.Render();Check(Changed(spotLit,h.Pixels())>50&&Energy(h.Pixels())<Energy(spotLit),"spot shadow projection and cone illuminate/occlude correctly");
    h.Scene.SpotLights.clear();h.Scene.RemoveObject(cube);lv::RectLight rect;rect.Position={0,0,2};rect.Direction={0,0,-1};rect.Up={0,1,0};rect.Intensity=2;rect.Width=rect.Height=2;rect.Range=10;h.Scene.RectLights={rect};h.Render();const auto area=h.Pixels();Check(Energy(area)>Energy(dark)+100000,"LTC rectangle produces area illumination");h.Scene.RectLights[0].Direction={0,0,1};h.Render();Check(Energy(h.Pixels())<Energy(area)/10,"one-sided rectangle rejects the back face");h.Scene.RectLights[0].TwoSided=true;h.Render();Check(Changed(area,h.Pixels())<10,"two-sided rectangle restores back-side illumination");
    h.Scene.RectLights[0].Width=.2f;h.Render();Check(Energy(h.Pixels())<Energy(area),"rectangle dimensions affect integrated energy");h.Scene.RectLights[0]=rect;h.Quality.Overrides.Pbr=true;auto material=h.Scene.GetMaterial(floor,0);material.Shading=lv::ShadingModel::Pbr;material.MetallicFactor=.8f;material.RoughnessFactor=.15f;h.Scene.SetMaterial(floor,0,material);h.Renderer.SetQuality(h.Quality,h.Scene).Value();h.Render();const auto glossy=h.Pixels();Check(Energy(glossy)>100000,"LTC GGX specular and diffuse paths render in PBR");material.RoughnessFactor=.8f;h.Scene.SetMaterial(floor,0,material);h.Render();Check(Changed(glossy,h.Pixels())>100,"LTC GGX lookup responds to material roughness");Check(h.Resource("Rect LTC matrix").Bytes==64*64*8,"rectangle lookup textures use binary16 coefficients");
    h.Quality.Overrides.OnDemand=lv::OnDemandMode::On;h.Renderer.SetQuality(h.Quality,h.Scene).Value();h.Render(false);Check(!h.Render(false).SceneRendered,"unchanged lighting keeps the On-Demand image");h.Scene.Ambient.Intensity=.1f;Check(h.Render(false).SceneRendered,"ambient edits invalidate On-Demand");h.Scene.RectLights[0].Position.X+=.5f;Check(h.Render(false).SceneRendered,"rectangle edits invalidate On-Demand");h.Scene.PointLights={point};h.Render(false);h.Scene.PointLights[0].Shadow.Enabled=true;Check(h.Render(false).SceneRendered,"local shadow enable invalidates On-Demand");
    CasterVariants(h);}
    PointFaceBoundaries();PcssAndHdr();PanoramaAndTransparentMeshes();
}catch(const std::exception& e){Check(false,e.what());}return Finish();}
int main(int argc,char** argv){return GraphicsScope([&]{return RunLightingPixels(argc,argv);});}
