#include <minvex/SceneIO.h>
#include <minvex/Telemetry.h>
#include <minvex/Version.h>
#include <cstdio>
int main(int argc,char** argv) {try {
    if(argc!=2){return 1;}
    lv::Scene scene;scene.Sun.Ambient=0;scene.Ambient={{.2f,.3f,.5f},.25f};
    lv::PointLight point;point.Position={0,2,0};point.Shadow.Enabled=true;scene.PointLights.push_back(point);
    lv::SpotLight spot;spot.Position={0,3,0};spot.Shadow.Enabled=true;scene.SpotLights.push_back(spot);
    lv::RectLight rect;rect.Position={0,4,0};rect.Width=3;rect.Height=2;scene.RectLights.push_back(rect);scene.LocalShadowResolution=256;scene.ValidateLighting();
    lv::SaveScene(scene,argv[1]).Value();lv::AssetRegistry registry;auto restored=lv::LoadScene(argv[1],registry).Value();
    if(restored.World.PointLights.size()!=1||!restored.World.PointLights[0].Shadow.Enabled||restored.World.RectLights.size()!=1||restored.World.LocalShadowResolution!=256||restored.World.Ambient.Intensity!=.25f){return 1;}
    std::printf("Installed Minvex %.*s lighting types and Scene v13 passed.\n",static_cast<int>(lv::Version.size()),lv::Version.data());return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
