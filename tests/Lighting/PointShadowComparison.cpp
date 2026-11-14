// Experimental dual-paraboloid comparison only; production uses projective cube faces.
#include <minvex/Scene.h>
#include "renderer/LightMath.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace {
constexpr unsigned Size=512;constexpr float Range=12;
void Hr(HRESULT hr){if(FAILED(hr)){throw std::runtime_error("D3D11 shadow comparison failed: "+std::to_string(static_cast<unsigned>(hr)));}}
using V=lv::Vec3;
V Add(V a,V b){return {a.X+b.X,a.Y+b.Y,a.Z+b.Z};}V Sub(V a,V b){return {a.X-b.X,a.Y-b.Y,a.Z-b.Z};}V Mul(V a,float s){return {a.X*s,a.Y*s,a.Z*s};}float Dot(V a,V b){return a.X*b.X+a.Y*b.Y+a.Z*b.Z;}
const char* Hlsl=R"(
cbuffer Params:register(b0){float4x4 light_matrix;float4 mode;};
struct Input{float3 p:POSITION;};struct Vertex{float4 p:SV_Position;float3 world:TEXCOORD0;};
Vertex VS(Input v){Vertex o;o.world=v.p;if(mode.x<.5){o.p=mul(light_matrix,float4(v.p,1));}else{float r=length(v.p);o.p=float4(v.p.xy/max(r+abs(v.p.z),1e-6),r/12.0,1);}return o;}
float CubePS(Vertex v):SV_Target0{return length(v.world)/12.0;}
struct DualOutput{float depth:SV_Depth;float color:SV_Target0;};
DualOutput DualPS(Vertex v){if(v.world.z*mode.y<0){discard;}DualOutput o;o.depth=o.color=length(v.world)/12.0;return o;}
)";
struct Maps {
    unsigned Faces;
    ComPtr<ID3D11Texture2D> Color,Depth,Staging;
    std::vector<ComPtr<ID3D11RenderTargetView>> Targets;
    std::vector<ComPtr<ID3D11DepthStencilView>> Depths;
    Maps(ID3D11Device* device,unsigned faces):Faces(faces),Targets(faces),Depths(faces){
        D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=Size;d.MipLevels=1;d.ArraySize=faces;d.Format=DXGI_FORMAT_R32_FLOAT;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET;Hr(device->CreateTexture2D(&d,nullptr,&Color));d.Format=DXGI_FORMAT_D32_FLOAT;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;Hr(device->CreateTexture2D(&d,nullptr,&Depth));
        d.Format=DXGI_FORMAT_R32_FLOAT;d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Hr(device->CreateTexture2D(&d,nullptr,&Staging));
        for(unsigned f=0;f<faces;++f){D3D11_RENDER_TARGET_VIEW_DESC r{};r.Format=DXGI_FORMAT_R32_FLOAT;r.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;r.Texture2DArray.FirstArraySlice=f;r.Texture2DArray.ArraySize=1;Hr(device->CreateRenderTargetView(Color.Get(),&r,&Targets[f]));D3D11_DEPTH_STENCIL_VIEW_DESC z{};z.Format=DXGI_FORMAT_D32_FLOAT;z.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2DARRAY;z.Texture2DArray.FirstArraySlice=f;z.Texture2DArray.ArraySize=1;Hr(device->CreateDepthStencilView(Depth.Get(),&z,&Depths[f]));}
    }
    auto Read(ID3D11DeviceContext* c){c->CopyResource(Staging.Get(),Color.Get());std::vector<float> result(std::size_t(Faces)*Size*Size);for(unsigned f=0;f<Faces;++f){D3D11_MAPPED_SUBRESOURCE m{};Hr(c->Map(Staging.Get(),f, D3D11_MAP_READ,0,&m));for(unsigned y=0;y<Size;++y){std::memcpy(result.data()+(std::size_t(f)*Size+y)*Size,static_cast<const char*>(m.pData)+y*m.RowPitch,Size*sizeof(float));}c->Unmap(Staging.Get(),f);}return result;}
};
struct Renderer {
    ComPtr<ID3D11Device> Device;ComPtr<ID3D11DeviceContext> Context;
    ComPtr<ID3D11VertexShader> Vs;ComPtr<ID3D11PixelShader> CubePs,DualPs;ComPtr<ID3D11InputLayout> Layout;
    ComPtr<ID3D11Buffer> Constants,Vertices;ComPtr<ID3D11RasterizerState> Raster;ComPtr<ID3D11DepthStencilState> Depth;
    std::array<lv::Mat4,6> Matrices;unsigned VertexCount=0;
    Renderer(){D3D_FEATURE_LEVEL feature;Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&Device,&feature,&Context));
        const auto compile=[&](const char* name,const char* profile){ComPtr<ID3DBlob> code,error;auto hr=D3DCompile(Hlsl,std::strlen(Hlsl),nullptr,nullptr,nullptr,name,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);if(FAILED(hr)&&error){std::fprintf(stderr,"%s\n",static_cast<const char*>(error->GetBufferPointer()));}Hr(hr);return code;};
        auto vs=compile("VS","vs_5_0"),cube=compile("CubePS","ps_5_0"),dual=compile("DualPS","ps_5_0");Hr(Device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&Vs));Hr(Device->CreatePixelShader(cube->GetBufferPointer(),cube->GetBufferSize(),nullptr,&CubePs));Hr(Device->CreatePixelShader(dual->GetBufferPointer(),dual->GetBufferSize(),nullptr,&DualPs));
        D3D11_INPUT_ELEMENT_DESC e{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};Hr(Device->CreateInputLayout(&e,1,vs->GetBufferPointer(),vs->GetBufferSize(),&Layout));D3D11_BUFFER_DESC b{};b.ByteWidth=80;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;b.Usage=D3D11_USAGE_DYNAMIC;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;Hr(Device->CreateBuffer(&b,nullptr,&Constants));
        D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=true;Hr(Device->CreateRasterizerState(&r,&Raster));D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=true;z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;z.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;Hr(Device->CreateDepthStencilState(&z,&Depth));lv::PointLight point;point.Range=Range;for(unsigned f=0;f<6;++f){Matrices[f]=lv::detail::PointShadowMatrix(point,f);}
    }
    void Mesh(const std::vector<V>& vertices){VertexCount=static_cast<unsigned>(vertices.size());Vertices.Reset();D3D11_BUFFER_DESC b{};b.ByteWidth=VertexCount*sizeof(V);b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.Usage=D3D11_USAGE_IMMUTABLE;D3D11_SUBRESOURCE_DATA d{};d.pSysMem=vertices.data();Hr(Device->CreateBuffer(&b,&d,&Vertices));}
    void Render(Maps& maps,bool dual){auto* c=Context.Get();const unsigned stride=sizeof(V),offset=0;c->IASetInputLayout(Layout.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);auto* vertex=Vertices.Get();c->IASetVertexBuffers(0,1,&vertex,&stride,&offset);c->VSSetShader(Vs.Get(),nullptr,0);c->PSSetShader(dual ? DualPs.Get():CubePs.Get(),nullptr,0);auto* constants=Constants.Get();c->VSSetConstantBuffers(0,1,&constants);c->PSSetConstantBuffers(0,1,&constants);c->RSSetState(Raster.Get());c->OMSetDepthStencilState(Depth.Get(),0);D3D11_VIEWPORT viewport{0,0,float(Size),float(Size),0,1};c->RSSetViewports(1,&viewport);
        for(unsigned f=0;f<maps.Faces;++f){const float clear[]{1,1,1,1};c->ClearRenderTargetView(maps.Targets[f].Get(),clear);c->ClearDepthStencilView(maps.Depths[f].Get(),D3D11_CLEAR_DEPTH,1,0);auto* target=maps.Targets[f].Get();c->OMSetRenderTargets(1,&target,maps.Depths[f].Get());struct Params{lv::Mat4 Matrix;float Mode[4];} p{dual ? lv::Mat4::Identity():Matrices[f],{dual ? 1.0f:0.0f,f==0 ? 1.0f:-1.0f,0,0}};D3D11_MAPPED_SUBRESOURCE m{};Hr(c->Map(Constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m));std::memcpy(m.pData,&p,sizeof(p));c->Unmap(Constants.Get(),0);c->Draw(VertexCount,0);}
        c->OMSetRenderTargets(0,nullptr,nullptr);
    }
    struct Timing {double CpuMs=0,GpuMs=0;};
    Timing Measure(Maps& maps,bool dual){const auto warm=std::chrono::steady_clock::now();unsigned n=0;do{Render(maps,dual);++n;}while(n<100||std::chrono::steady_clock::now()-warm<std::chrono::milliseconds(500));
        ComPtr<ID3D11Query> disjoint,start,end;D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};Hr(Device->CreateQuery(&q,&disjoint));q.Query=D3D11_QUERY_TIMESTAMP;Hr(Device->CreateQuery(&q,&start));Hr(Device->CreateQuery(&q,&end));constexpr unsigned frames=600;
        auto* c=Context.Get();c->Begin(disjoint.Get());c->End(start.Get());const auto time=std::chrono::steady_clock::now();for(unsigned f=0;f<frames;++f){Render(maps,dual);}c->End(end.Get());c->End(disjoint.Get());c->Flush();D3D11_QUERY_DATA_TIMESTAMP_DISJOINT freq{};UINT64 a=0,b=0;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(c->GetData(disjoint.Get(),&freq,sizeof(freq),0)==S_FALSE){if(std::chrono::steady_clock::now()>deadline){throw std::runtime_error("GPU query timeout");}}while(c->GetData(end.Get(),&b,sizeof(b),0)==S_FALSE){}Hr(c->GetData(start.Get(),&a,sizeof(a),0));if(freq.Disjoint||!freq.Frequency){throw std::runtime_error("Invalid GPU timestamps");}return {std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-time).count()/frames,double(b-a)*1000.0/double(freq.Frequency)/frames};
    }
};
void Quad(std::vector<V>& mesh,std::array<V,4> p,unsigned subdivisions){const auto vertex=[&](unsigned x,unsigned y){float u=float(x)/subdivisions,v=float(y)/subdivisions;return Add(Add(Mul(p[0],(1-u)*(1-v)),Mul(p[1],u*(1-v))),Add(Mul(p[2],u*v),Mul(p[3],(1-u)*v)));};for(unsigned y=0;y<subdivisions;++y){for(unsigned x=0;x<subdivisions;++x){V a=vertex(x,y),b=vertex(x+1,y),c=vertex(x+1,y+1),d=vertex(x,y+1);for(V q:{a,b,c,a,c,d}){mesh.push_back(q);}}}}
auto Fixture(unsigned kind,unsigned subdivisions){std::vector<V> mesh;if(kind==1){Quad(mesh,{{{-4,-3,1},{4,-3,2},{4,3,5},{-4,3,4}}},subdivisions);}else{const V center=kind==0 ? V{0,0,3}:V{3,0,.1f};const float h=1.2f;for(unsigned axis=0;axis<3;++axis){for(int sign:{-1,1}){std::array<V,4> corners;for(unsigned k=0;k<4;++k){float p[3]{};p[axis]=h*sign;p[(axis+1)%3]=(k==1||k==2) ? h:-h;p[(axis+2)%3]=k>=2 ? h:-h;corners[k]=Add(center,{p[0],p[1],p[2]});}Quad(mesh,corners,subdivisions);}}}return mesh;}
float Intersect(V ray,V a,V b,V c){const auto e1=Sub(b,a),e2=Sub(c,a),p=lv::detail::LightCross(ray,e2);const float det=Dot(e1,p);if(std::abs(det)<1e-7f){return std::numeric_limits<float>::infinity();}const float inverse=1/det;const auto t=Mul(a,-1);const float u=Dot(t,p)*inverse;if(u<0||u>1){return std::numeric_limits<float>::infinity();}const auto q=lv::detail::LightCross(t,e1);const float v=Dot(ray,q)*inverse;if(v<0||u+v>1){return std::numeric_limits<float>::infinity();}const float distance=Dot(e2,q)*inverse;return distance>.05f ? distance:std::numeric_limits<float>::infinity();}
unsigned CubeFace(V v){const V a{std::abs(v.X),std::abs(v.Y),std::abs(v.Z)};if(a.X>=a.Y&&a.X>=a.Z){return v.X>=0 ? 0:1;}if(a.Y>=a.Z){return v.Y>=0 ? 2:3;}return v.Z>=0 ? 4:5;}
struct Errors {unsigned Probes=0,FalseLit=0,FalseShadow=0;};
Errors Compare(const std::vector<V>& mesh,const std::vector<float>& maps,bool dual,const std::array<lv::Mat4,6>& matrices){Errors result;constexpr unsigned probes=8192;for(unsigned i=0;i<probes;++i){const float z=1-2*(float(i)+.5f)/probes,r=std::sqrt(std::max(0.0f,1-z*z)),angle=float(i)*2.39996323f;const V ray{r*std::cos(angle),r*std::sin(angle),z};float nearest=Range;for(std::size_t t=0;t<mesh.size();t+=3){nearest=std::min(nearest,Intersect(ray,mesh[t],mesh[t+1],mesh[t+2]));}const bool truth=nearest<Range*.94f;unsigned face;float x,y;if(dual){face=ray.Z>=0 ? 0:1;const float denominator=1+std::abs(ray.Z);x=ray.X/denominator;y=ray.Y/denominator;}else{face=CubeFace(ray);const auto& m=matrices[face].Elements;const float w=m[3]*ray.X+m[7]*ray.Y+m[11]*ray.Z;x=(m[0]*ray.X+m[4]*ray.Y+m[8]*ray.Z)/w;y=(m[1]*ray.X+m[5]*ray.Y+m[9]*ray.Z)/w;}const int px=std::clamp(int((x*.5f+.5f)*Size),0,int(Size)-1),py=std::clamp(int((.5f-y*.5f)*Size),0,int(Size)-1);const float depth=maps[(std::size_t(face)*Size+py)*Size+px];const bool shadow=depth<.94f;result.FalseLit+=truth&&!shadow;result.FalseShadow+=!truth&&shadow;++result.Probes;}return result;}
}
int main(int argc,char** argv){try{if(argc!=2){throw std::invalid_argument("Pass output.json");}Renderer renderer;Maps cube(renderer.Device.Get(),6),dual(renderer.Device.Get(),2);std::ofstream out(argv[1]);out<<std::setprecision(12)<<"{\"format\":\"MinvexPointShadowComparison\",\"version\":1,\"resolution\":512,\"frames\":600,\"warmupMs\":500,\"scope\":\"uncached opaque static geometry, same triangles; dual projection is an experimental quality baseline, not shipped\",\"cases\":[";bool first=true;
    for(unsigned kind=0;kind<3;++kind){for(unsigned divisions:{1u,16u}){const auto mesh=Fixture(kind,divisions);renderer.Mesh(mesh);auto cubeTime=renderer.Measure(cube,false),dualTime=renderer.Measure(dual,true);renderer.Render(cube,false);renderer.Render(dual,true);auto ce=Compare(mesh,cube.Read(renderer.Context.Get()),false,renderer.Matrices),de=Compare(mesh,dual.Read(renderer.Context.Get()),true,renderer.Matrices);if(!first){out<<',';}first=false;const char* names[]{"front-box","large-oblique-quad","equator-box"};out<<"{\"name\":\""<<names[kind]<<"\",\"subdivisions\":"<<divisions<<",\"triangles\":"<<mesh.size()/3<<",\"cube\":{\"completedMs\":"<<cubeTime.CpuMs<<",\"gpuBatchMs\":"<<cubeTime.GpuMs<<",\"bytes\":"<<6ull*Size*Size*8<<",\"falseLit\":"<<ce.FalseLit<<",\"falseShadow\":"<<ce.FalseShadow<<"},\"dual\":{\"completedMs\":"<<dualTime.CpuMs<<",\"gpuBatchMs\":"<<dualTime.GpuMs<<",\"bytes\":"<<2ull*Size*Size*8<<",\"falseLit\":"<<de.FalseLit<<",\"falseShadow\":"<<de.FalseShadow<<"},\"rayProbes\":"<<ce.Probes<<'}';std::printf("%s/%u: cube %.4f ms error %u/%u, dual %.4f ms error %u/%u\n",names[kind],divisions,cubeTime.CpuMs,ce.FalseLit+ce.FalseShadow,ce.Probes,dualTime.CpuMs,de.FalseLit+de.FalseShadow,de.Probes);}}
    out<<"]}\n";out.close();if(!out){throw std::runtime_error("Cannot save comparison");}return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
