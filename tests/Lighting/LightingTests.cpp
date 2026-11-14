#include "TestHarness.h"
#include <minvex/EditorDocument.h>
#include <minvex/SceneIO.h>
#include <minvex/Telemetry.h>
#include "renderer/LightMath.h"
#include <json.hpp>
#include <limits>
using namespace test;
int main() {try {
    lv::Scene scene;scene.ValidateLighting();Check(scene.Ambient.Intensity==0&&scene.LocalShadowResolution==512,"new lighting preserves legacy default appearance");
    lv::AmbientLight ambient{{.2f,.4f,1},.7f};ambient.Validate();
    Rejects([]{lv::AmbientLight{{-1,0,0},1}.Validate();},"negative ambient color is rejected");
    Rejects([]{lv::AmbientLight{{1,1,1},std::numeric_limits<float>::infinity()}.Validate();},"nonfinite ambient is rejected");
    lv::PointLight point;point.Shadow.Enabled=true;point.Validate();
    Rejects([&]{auto l=point;l.Shadow.NearPlane=l.Range;l.Validate();},"point near plane must precede range");
    Rejects([&]{auto l=point;l.Shadow.Filter=static_cast<lv::LocalShadowFilter>(9);l.Validate();},"unknown local filter is rejected");
    lv::SpotLight spot;spot.Shadow.Enabled=true;spot.Direction={.2f,-1,.1f};spot.Validate();
    lv::RectLight rect;rect.Position={0,3,0};rect.Validate();const auto axes=lv::detail::RectAxes(rect);
    Check(Near(std::hypot(axes[0].X,axes[0].Y,axes[0].Z),1)&&Near(std::hypot(axes[1].X,axes[1].Y,axes[1].Z),1),"rectangle axes are normalized");
    Rejects([&]{auto l=rect;l.Up=l.Direction;l.Validate();},"collinear rectangle axes are rejected");
    Rejects([&]{auto l=rect;l.Width=0;l.Validate();},"zero area rectangle is rejected");
    bool scaledAxesFinite=true;for(float scale:{1e-38f,3e38f}){auto l=rect;l.Up={0,scale,scale};l.Direction={0,-1,1};l.Validate();for(auto axis:lv::detail::RectAxes(l)){scaledAxesFinite&=std::isfinite(axis.X)&&std::isfinite(axis.Y)&&std::isfinite(axis.Z)&&Near(std::hypot(axis.X,axis.Y,axis.Z),1);}}Check(scaledAxesFinite,"rectangle axes remain finite for very small and large valid up vectors");
    scene.PointLights.assign(5,point);Rejects([&]{scene.ValidateLighting();},"shadowed local light budget is enforced");scene.PointLights.clear();scene.RectLights.assign(5,rect);Rejects([&]{scene.ValidateLighting();},"rectangle light budget is enforced");scene.RectLights.clear();scene.PointLights.assign(8,{});scene.RectLights.push_back(rect);Rejects([&]{scene.ValidateLighting();},"combined local budget includes rectangle lights");
    scene.PointLights={point};scene.SpotLights={spot};scene.RectLights={rect};scene.Ambient=ambient;scene.LocalShadowResolution=256;scene.ValidateLighting();
    const auto layout=lv::detail::ShadowLayout(scene);Check(layout.Faces==7&&layout.Columns==6&&layout.Rows==2&&layout.Bytes()==6ull*2*256*256*8,"shared atlas layout and attachment bytes include point six faces and spot one");
    lv::QualityCapabilities caps;caps.GraphicsAvailable=caps.ShadowMapping=caps.LocalShadowMapping=caps.RectAreaLighting=true;caps.MaxTextureSize=8192;auto quality=lv::MakeQualityProfile(lv::QualityPreset::Low);quality.Overrides.Lighting=lv::LightingMode::Advanced;quality.Overrides.Shadow=lv::ShadowMode::Basic;auto estimate=lv::EstimateQualityResources(quality,caps,scene,128,128);Check(estimate&&estimate.Value().LocalShadowFaces==7&&estimate.Value().Cost.RenderTargetBytes==lv::RenderTargetBytes(*estimate.Value().Quality.Effective,128,128)+layout.Bytes()&&estimate.Value().Cost.TextureBytes==65536,"resource estimates include atlas attachments and shared LTC tables");
    if(estimate){auto repeat=lv::EstimateQualityResources(quality,caps,scene,128,128,&estimate.Value());Check(repeat&&repeat.Value().AdditionalBytes==0,"unchanged lighting allocation estimates reuse existing resources");}
    caps.LocalShadowMapping=false;auto unsupported=lv::EstimateQualityResources(quality,caps,scene,128,128);Check(!unsupported&&unsupported.Error().Code==lv::ErrorCode::UnsupportedFeature,"local shadow capability is required independently of Sun shadow capability");caps.LocalShadowMapping=true;
    caps.RenderTargetBudgetBytes=4*1024*1024;auto overBudget=lv::EstimateQualityResources(quality,caps,scene,128,128);Check(!overBudget&&overBudget.Error().Code==lv::ErrorCode::InsufficientResource,"local shadow atlas is included in the configured attachment budget");
    bool finite=true;for(unsigned face=0;face<6;++face){const auto matrix=lv::detail::PointShadowMatrix(point,face);for(float value:matrix.Elements){finite&=std::isfinite(value);}}Check(finite,"all cube shadow projection matrices are finite");
    (void)lv::detail::SpotShadowMatrix(spot);
    auto distant=point;distant.Position={1e8f,1e8f,1e8f};bool stable=true;for(unsigned f=0;f<6;++f){for(float v:lv::detail::PointShadowMatrix(distant,f).Elements){stable&=std::isfinite(v);}}auto distantSpot=spot;distantSpot.Position=distant.Position;for(float v:lv::detail::SpotShadowMatrix(distantSpot).Elements){stable&=std::isfinite(v);}Check(stable,"distant light orientation avoids position-plus-unit-vector cancellation");
    lv::EditorDocument doc;Check(doc.SetAmbientLight(ambient)&&doc.Undo()&&doc.GetScene().Ambient.Intensity==0&&doc.Redo(),"ambient edit supports Undo/Redo");
    Check(doc.SetLocalLights({point},{spot},{rect})&&doc.SetLocalLights({point},{spot})&&doc.GetScene().RectLights.size()==1,"legacy local setter preserves rectangle lights");
    Check(doc.SetLocalShadowResolution(256)&&doc.Undo()&&doc.GetScene().LocalShadowResolution==512&&doc.Redo(),"local shadow resolution participates in history");
    Check(!doc.SetLocalShadowResolution(777)&&doc.GetScene().LocalShadowResolution==256,"invalid shadow resolution is atomic");
    const auto path=TemporaryDirectory();const auto file=path.File("Lighting.mvxscene.json");Check(bool(doc.Save(file)),"Scene v13 saves complete lighting");lv::EditorDocument restored;Check(bool(restored.Open(file)),"Scene v13 restores complete lighting");
    const auto& world=restored.GetScene();Check(world.Ambient.Color.Z==1&&world.Ambient.Intensity==ambient.Intensity&&world.RectLights.size()==1&&world.RectLights[0].Width==2&&world.PointLights[0].Shadow.Enabled&&world.SpotLights[0].Shadow.Enabled&&world.LocalShadowResolution==256,"lighting values and local shadow parameters round trip");
    auto json=nlohmann::json::parse(Read(file));Check(json["version"]==13,"writer version is 13");json["version"]=12;Write(file,json.dump());lv::AssetRegistry registry;auto legacy=lv::LoadScene(file,registry);Check(legacy&&legacy.Value().World.Ambient.Intensity==0&&legacy.Value().World.RectLights.empty()&&!legacy.Value().World.PointLights[0].Shadow.Enabled,"v12 retains original lighting defaults without reading future fields");
    json["version"]=13;json["pointLights"][0]["shadow"]["filter"]="typo";Write(file,json.dump());Check(!lv::LoadScene(file,registry),"unknown stored shadow filtering is rejected");
}catch(const std::exception& e){Check(false,e.what());}return Finish();}
