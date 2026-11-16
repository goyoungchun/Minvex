// Included inside the pixel harness namespace; both native backends run these
// image assertions through the same Host and normalized RGBA readback.
void PointFaceBoundaries() {
    Host h(128,128);h.Scene.MainCamera.OrthographicHeight=4;
    const auto floor=h.Scene.AddModel(Floor());const auto cube=h.Scene.AddModel(Model(lv::MakeCube()));
    lv::PointLight light;light.Intensity=8;light.Range=10;light.Shadow.Enabled=true;h.Scene.PointLights={light};
    // The projected rig stays identical across twelve cube-face edges and
    // eight corners. A missing face/tile must not become a bright seam.
    for(int x=-1;x<=1;++x) {for(int y=-1;y<=1;++y) {for(int z=-1;z<=1;++z) {
        const int axes=(x!=0)+(y!=0)+(z!=0);if(axes<2) {continue;}
        const float length=std::sqrt(float(x*x+y*y+z*z));const lv::Vec3 normal{float(x)/length,float(y)/length,float(z)/length};
        auto matrix=lv::Mat4::LookAt(normal,{},std::abs(normal.Y)>.9f ? lv::Vec3{1,0,0}:lv::Vec3{0,1,0}).InverseAffine();
        matrix.Elements[12]=matrix.Elements[13]=matrix.Elements[14]=0;
        auto receiver=lv::DecomposeTRS(matrix);h.Scene.SetTransform(floor,receiver);
        auto caster=receiver;caster.Position=matrix.TransformPoint({0,0,1});caster.Scale={.4f,.4f,.4f};h.Scene.SetTransform(cube,caster);
        // View the receiver shadow from the side so the caster itself cannot
        // cover the pixels used to detect a seam through the shadow center.
        h.Scene.MainCamera.Position=matrix.TransformPoint({4,0,6});h.Scene.MainCamera.Up=matrix.TransformPoint({0,1,0});
        auto& point=h.Scene.PointLights[0];point.Position=matrix.TransformPoint({0,0,3});point.Shadow.DepthBias=20;h.Render();const auto lit=h.Pixels();
        point.Shadow.DepthBias=.005f;h.Render();const auto shadow=h.Pixels();
        bool continuous=true;
        for(int py=60;py<68;++py) {for(int px=60;px<68;++px) {const auto i=(py*128+px)*4;continuous&=lit[i]>80 && shadow[i]<lit[i]/2;}}
        if(!continuous) {std::printf("Boundary (%d,%d,%d): center lit=%u shadow=%u\n",x,y,z,unsigned(lit[(64*128+64)*4]),unsigned(shadow[(64*128+64)*4]));}
        Check(continuous,axes==2 ? "point shadow has no light leak across a cube-face edge":"point shadow has no light leak at a three-face corner");
    }}}
}
void ReferenceImage(const char* name,const std::vector<std::uint8_t>& pixels,bool update) {
    const auto path=Fixture("Lighting")/(std::string(name)+".rgba");
    if(update) {std::filesystem::create_directories(path.parent_path());Write(path,{reinterpret_cast<const char*>(pixels.data()),pixels.size()});return;}
    const auto expected=Read(path);if(expected.size()!=pixels.size()){throw std::runtime_error("Reference image extent differs: "+path.string());}
    std::uint64_t delta=0;std::size_t changed=0;bool alpha=true;
    for(std::size_t i=0;i<pixels.size();i+=4){int maximum=0;for(int c=0;c<3;++c){const auto d=std::abs(int(pixels[i+c])-int(static_cast<std::uint8_t>(expected[i+c])));maximum=std::max(maximum,d);delta+=d;}changed+=maximum>24;alpha&=pixels[i+3]==static_cast<std::uint8_t>(expected[i+3]);}
    const double mean=double(delta)/(pixels.size()/4*3),fraction=double(changed)/(pixels.size()/4);
    if(mean>1||fraction>.02||!alpha){std::printf("Reference %s: mean %.5f, changed %.5f, alpha=%d\n",name,mean,fraction,alpha);}
    Check(mean<=1&&fraction<=.02&&alpha,"PCSS/HDR pixels match the frozen native reference within documented tolerance");
}
void PcssAndHdr(bool updateReferences=false) {
    Host h(128,128);h.Scene.MainCamera.Position={0,0,5};
    h.Scene.Sun.Direction={2,0,-1};h.Scene.Sun.Intensity=2.236068f;h.Scene.Sun.Ambient=0;
    const auto floor=h.Scene.AddModel(Floor(2)),caster=h.Scene.AddModel(Floor(.3f));
    const auto place=[&](float height) {lv::Transform t;t.Position={-2*height,0,height};h.Scene.SetTransform(caster,t);};place(.3f);
    h.Quality=lv::MakeQualityProfile(lv::QualityPreset::Medium);auto& o=h.Quality.Overrides;
    o.TargetFps=0;o.OnDemand=lv::OnDemandMode::On;o.ShadowResolution=1024;o.Instancing=false;
    o.Shadow=lv::ShadowMode::Pcss;o.ShadowSourceAngle=20.f;o.ShadowMaxFilterRadius=64.f;o.ShadowBlockerSamples=32;o.ShadowFilterSamples=64;
    const auto apply=[&] {h.Renderer.SetQuality(h.Quality,h.Scene).Value();};
    const auto render=[&] {Check(h.Render(false).SceneRendered,"PCSS edits invalidate the cached image");return h.Pixels();};
    const auto debug=[&](lv::DebugView view) {h.Renderer.SetDebugOptions({view}).Value();return render();};
    const auto red=[](const auto& pixels,int x,int y) {return pixels[(y*128+x)*4];};
    const auto peak=[&](const auto& image) {int value=0;for(int y=52;y<76;++y) {for(int x=54;x<80;++x) {value=std::max(value,int(red(image,x,y)));}}return value;};
    const auto partial=[&](const auto& image) {int count=0;for(int y=48;y<80;++y) {for(int x=52;x<84;++x) {const auto v=red(image,x,y);count+=v>12 && v<240;}}return count;};
    apply();const auto nearImage=render();Check(h.Renderer.GetQuality().Value().Effective->Shadow==lv::ShadowMode::Pcss && h.ShadowCalls()==2,"native PCSS executes the Sun shadow pass");
    const auto nearRadius=debug(lv::DebugView::ShadowPenumbra);place(1);const auto farRadius=render();
    Check(peak(farRadius)>peak(nearRadius)+5,"PCSS penumbra grows with receiver-blocker distance");
    const auto blockers=debug(lv::DebugView::ShadowBlockers),depth=debug(lv::DebugView::ShadowBlockerDepth),visibility=debug(lv::DebugView::ShadowVisibility);
    Check(red(blockers,64,64)>0 && red(depth,64,64)<250 && red(visibility,64,64)<100,"PCSS diagnostics expose real blockers and occlusion");
    Check(red(blockers,110,24)==0 && red(depth,110,24)==255 && red(visibility,110,24)>250,"PCSS no-blocker pixels remain fully lit with the depth sentinel");
    const auto farImage=debug(lv::DebugView::Lit);Check(partial(farImage)>partial(nearImage)+8,"far PCSS shadows have a wider transition than contact shadows");
    ReferenceImage("pcss-near",nearImage,updateReferences);ReferenceImage("pcss-far",farImage,updateReferences);
    ReferenceImage("pcss-near-radius",nearRadius,updateReferences);ReferenceImage("pcss-far-radius",farRadius,updateReferences);
    ReferenceImage("pcss-blockers",blockers,updateReferences);ReferenceImage("pcss-depth",depth,updateReferences);ReferenceImage("pcss-visibility",visibility,updateReferences);
    o.Shadow=lv::ShadowMode::Pcf;apply();Check(partial(farImage)>partial(render())+8,"PCSS is softer than fixed PCF away from contact");
    o.Shadow=lv::ShadowMode::Basic;apply();const auto hard=render();o.Shadow=lv::ShadowMode::Pcss;o.ShadowSourceAngle=0.f;apply();Check(render()==hard,"zero-angle PCSS exactly reproduces hard shadows");
    o.ShadowSourceAngle=20.f;o.ShadowMaxFilterRadius=1.f;apply();Check(peak(debug(lv::DebugView::ShadowPenumbra))>240,"PCSS radius respects its configured texel clamp");
    (void)debug(lv::DebugView::Lit);o.ShadowMaxFilterRadius=64.f;apply();
    for(int samples:{8,32,64}) {o.ShadowBlockerSamples=samples;o.ShadowFilterSamples=samples;apply();const auto image=render();Check(red(image,64,64)<100 && red(image,110,24)>250,"PCSS sample-count variants preserve shadowed and unblocked pixels");}
    h.Scene.RemoveObject(caster);const auto clear=render();bool clean=true;for(int y=2;y<126;++y) {for(int x=2;x<126;++x) {clean&=red(clear,x,y)>250;}}
    Check(clean,"maximum-radius PCSS avoids self-shadow and map-edge darkening");Check(!h.Render(false).SceneRendered,"unchanged PCSS reuses its cached image");
    h.Scene.Sun.Intensity=0;h.Scene.Sun.Ambient=3;o.Shadow=lv::ShadowMode::Off;o.PostProcessing=false;apply();const auto clipped=render();
    o.PostProcessing=true;apply();const auto hdr=render();Check(red(clipped,64,64)==255 && red(hdr,64,64)>175 && red(hdr,64,64)<210,"HDR intermediate preserves and Reinhard maps light energy");
    ReferenceImage("hdr-clipped",clipped,updateReferences);ReferenceImage("hdr-tonemapped",hdr,updateReferences);
    h.Renderer.SetDebugOptions({lv::DebugView::Normal}).Value();const auto hdrNormal=render();o.PostProcessing=false;apply();
    Check(render()==hdrNormal,"normal diagnostics bypass HDR tone mapping and FXAA");
    ReferenceImage("hdr-normal",hdrNormal,updateReferences);
    (void)floor;
}

void ShaderVariants(const std::filesystem::path& output) {
    Host h(128,128);h.Scene.MainCamera.Target={0,1,0};h.Scene.MainCamera.Position={0,1,6};h.Scene.Sun.Intensity=.75f;h.Scene.Sun.Ambient=.25f;
    lv::AssetRegistry assets;
    const char* names[]={"rigid","skin","morph","morph-skin","instancing"};
    const char* files[]={nullptr,"AnimatedRibbon.gltf","MorphRigid.gltf","MorphRibbon.gltf",nullptr};
    const char* shadows[]={"off","basic","pcf","pcss"};
    std::ofstream out(output);if(!out){throw std::runtime_error("Cannot open shader variant report.");}
    out<<"{\"format\":\"MinvexShaderVariants\",\"width\":128,\"height\":128,\"cases\":[";
    unsigned cases=0,unsupported=0;const auto before=Failures;
    const auto capabilities=h.Renderer.GetQualityCapabilities().Value();
    for(int kind=0;kind<5;++kind) {
        for(auto id:h.Scene.ObjectIds()){if(h.Scene.Contains(id)){h.Scene.RemoveObject(id);}}
        auto model=files[kind] ? std::make_shared<lv::ModelAsset>(*assets.LoadModel(Fixture(files[kind]))):Model(lv::MakeCube());
        model->Images={{1,1,{128,128,255,255}}};model->Textures={{}};
        for(auto& material:model->Materials){material.Unlit=false;material.DoubleSided=true;material.BaseColor={1,1,1,1};material.NormalTexture=0;material.MetallicRoughnessTexture=0;}
        const auto first=h.Scene.AddModel(model);
        if(!model->Animations.empty()){auto& animation=h.Scene.GetAnimation(first);animation.Play(0);animation.Seek(.5);animation.Pause();}
        if(kind==4){lv::Transform t;t.Position.X=.8f;h.Scene.AddModel(model,t);t.Position.X=-.8f;h.Scene.SetTransform(first,t);}
        lv::PointLight point;point.Position={1,2,3};point.Intensity=4;point.Shadow.Enabled=true;h.Scene.PointLights={point};
        lv::SpotLight spot;spot.Position={-1,2,3};spot.Direction={1,-1,-3};spot.Shadow.Enabled=true;h.Scene.SpotLights={spot};
        lv::RectLight rect;rect.Position={0,2,3};rect.Direction={0,0,-1};rect.Up={0,1,0};h.Scene.RectLights={rect};
        for(int shadow=0;shadow<4;++shadow){for(bool pbr:{false,true}){for(bool post:{false,true}){for(int msaa:{1,4}) {
            if(cases++){out<<',';}
            out<<"{\"geometry\":\""<<names[kind]<<"\",\"shadow\":\""<<shadows[shadow]<<"\",\"pbr\":"<<(pbr ? "true":"false")<<",\"postProcessing\":"<<(post ? "true":"false")<<",\"msaa\":"<<msaa;
            const auto mask=post ? capabilities.HdrSampleCounts:capabilities.SampleCounts;
            if(!(mask&(1u<<msaa))) {++unsupported;out<<",\"status\":\"UNSUPPORTED\"}";continue;}
            const auto prior=Failures;
            try {
                // Invalidate the shadow cache for each case so a cache hit
                // cannot stand in for execution of its caster shader variant.
                for(auto id:h.Scene.ObjectIds()){auto transform=h.Scene.GetObject(id).LocalTransform;transform.Position.Z=float(cases)*.0001f;h.Scene.SetTransform(id,transform);}
                auto materials=std::make_shared<lv::ModelAsset>(*model);
                for(auto& material:materials->Materials){material.Shading=pbr ? lv::ShadingModel::Pbr:lv::ShadingModel::Basic;material.MetallicFactor=.4f;material.RoughnessFactor=.5f;}
                for(auto id:h.Scene.ObjectIds()) {
                    if(kind==4) {h.Scene.SetModel(id,materials);} // Shared immutable materials preserve batching eligibility.
                    else {for(std::size_t m=0;m<materials->Materials.size();++m){h.Scene.SetMaterial(id,m,materials->Materials[m]);}}
                }
                h.Quality=lv::MakeQualityProfile(lv::QualityPreset::Medium);auto& q=h.Quality.Overrides;
                q.Lighting=lv::LightingMode::Advanced;q.Pbr=pbr;q.PostProcessing=post;q.MsaaSamples=msaa;q.Shadow=static_cast<lv::ShadowMode>(shadow);q.ShadowResolution=512;q.Instancing=kind==4;q.TargetFps=0;q.OnDemand=lv::OnDemandMode::Off;
                const auto applied=h.Renderer.SetQuality(h.Quality,h.Scene);if(!applied){throw std::runtime_error(applied.Error().Message);}h.Render();const auto pixels=h.Pixels();
                std::size_t lit=0;for(std::size_t i=0;i<pixels.size();i+=4){lit+=pixels[i]+pixels[i+1]+pixels[i+2]>40;}
                const auto calls=h.ShadowCalls();Check(lit>20,"shader variant produces actual lit geometry pixels");Check(shadow==0 ? calls==0:calls>0,"shader variant executes the requested shadow path");
                const auto instanced=h.Renderer.GetTelemetry().Value().Optimization.InstancedObjects;
                if(kind==4){Check(instanced==2,"shader matrix executes the instanced vertex path");}
                out<<",\"litPixels\":"<<lit<<",\"shadowDrawCalls\":"<<calls;
                out<<",\"instancedObjects\":"<<instanced;
            }catch(const std::exception& e){std::fprintf(stderr,"Variant %s/%s/pbr=%d/post=%d/msaa=%d: ",names[kind],shadows[shadow],pbr,post,msaa);Check(false,e.what());}
            out<<",\"status\":\""<<(Failures==prior ? "PASS":"FAIL")<<"\"}";
        }}}}
    }
    out<<"],\"caseCount\":"<<cases<<",\"unsupported\":"<<unsupported<<",\"failures\":"<<Failures-before<<"}\n";out.close();if(!out){throw std::runtime_error("Cannot save shader variant report.");}
}
