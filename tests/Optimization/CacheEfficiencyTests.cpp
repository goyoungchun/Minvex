#include "TestHarness.h"
#include <minvex/Optimization.h>
#include "optimization/IndexPacking.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <numeric>
#include <random>
#include <set>
using namespace test;
namespace {
lv::MeshData Grid(unsigned n,bool shuffle) {
    lv::MeshData mesh;
    for (unsigned y=0;y<=n;++y) {for (unsigned x=0;x<=n;++x) {lv::Vertex v{};v.Position={float(x)/n*2-1,float(y)/n*2-1,0};v.Normal={0,0,1};v.Color={1,1,1,1};v.TexCoord={float(x)/n,float(y)/n};mesh.Vertices.push_back(v);}}
    std::vector<std::array<std::uint32_t,3>> triangles;
    for (unsigned y=0;y<n;++y) {for (unsigned x=0;x<n;++x) {const auto a=y*(n+1)+x;triangles.push_back({a,a+1,a+n+2});triangles.push_back({a,a+n+2,a+n+1});}}
    if (shuffle) {std::mt19937 random(19);std::shuffle(triangles.begin(),triangles.end(),random);}
    for (const auto& triangle:triangles) {mesh.Indices.insert(mesh.Indices.end(),triangle.begin(),triangle.end());}
    if (shuffle) {std::mt19937 random(41);std::vector<unsigned> order(mesh.Vertices.size());std::iota(order.begin(),order.end(),0);std::shuffle(order.begin(),order.end(),random);auto source=mesh.Vertices;for (std::size_t i=0;i<order.size();++i) {mesh.Vertices[order[i]]=source[i];}for (auto& index:mesh.Indices) {index=order[index];}}
    return mesh;
}
lv::MeshData Layers() {
    lv::MeshData mesh;const auto grid=Grid(16,false);
    for (unsigned layer=0;layer<8;++layer) {const auto base=static_cast<std::uint32_t>(mesh.Vertices.size());for (auto v:grid.Vertices) {v.Position[2]=float(layer)*.1f;mesh.Vertices.push_back(v);}for (auto index:grid.Indices) {mesh.Indices.push_back(index+base);}}
    return mesh;
}
std::vector<std::array<std::uint32_t,3>> Triangles(std::span<const std::uint32_t> indices,const std::vector<std::uint32_t>& inverse={}) {
    std::vector<std::array<std::uint32_t,3>> result;
    for (std::size_t i=0;i<indices.size();i+=3) {std::array<std::uint32_t,3> tri;for (unsigned c=0;c<3;++c) {tri[c]=inverse.empty() ? indices[i+c]:inverse.at(indices[i+c]);}result.push_back(tri);}std::sort(result.begin(),result.end());return result;
}
void Preserves(const lv::MeshData& input,const std::vector<lv::MeshLod>& lods,const lv::MeshOptimization& output) {
    Check(output.Geometry.Vertices.size()==input.Vertices.size()&&output.VertexRemap.size()==input.Vertices.size(),"optimization retains unused/source vertex population");
    std::vector<std::uint32_t> inverse(output.VertexRemap.size(),UINT32_MAX);bool attributes=true;
    for (std::size_t i=0;i<input.Vertices.size();++i) {const auto next=output.VertexRemap[i];if (next>=inverse.size()||inverse[next]!=UINT32_MAX) {attributes=false;break;}inverse[next]=static_cast<std::uint32_t>(i);attributes&=std::memcmp(&input.Vertices[i],&output.Geometry.Vertices[next],sizeof(lv::Vertex))==0;}
    Check(attributes,"fetch remap is bijective and preserves all UV/normal/color/joint/weight/tangent bytes");
    if (!attributes) {return;}
    Check(Triangles(input.Indices)==Triangles(output.Geometry.Indices,inverse),"triangle multiset and winding survive cache/overdraw/fetch passes");
    Check(output.Lods.size()==lods.size(),"LOD population remains intact");
    for (std::size_t i=0;i<lods.size();++i) {Check(output.Lods[i].Error==lods[i].Error&&Triangles(lods[i].Indices)==Triangles(output.Lods[i].Indices,inverse),"each LOD preserves its source triangles/error using the same vertex remap");}
}
void Stats(std::ostream& file,const lv::MeshLocalityStatistics& s) {file<<"{\"acmr\":"<<s.Acmr<<",\"atvr\":"<<s.Atvr<<",\"verticesTransformed\":"<<s.VerticesTransformed<<",\"bytesFetched\":"<<s.VertexBytesFetched<<",\"overfetch\":"<<s.Overfetch<<",\"overdraw\":"<<s.Overdraw<<",\"coveredPixels\":"<<s.PixelsCovered<<",\"shadedPixels\":"<<s.PixelsShaded<<'}';}
void Report(std::ostream& file,const char* name,const lv::MeshData& mesh,std::span<const lv::MeshLod> lods) {
    file<<"{\"case\":\""<<name<<"\",\"vertices\":"<<mesh.Vertices.size()<<",\"triangles\":"<<mesh.Indices.size()/3<<",\"vertexStride\":"<<sizeof(lv::Vertex)<<",\"raw\":";Stats(file,lv::AnalyzeMeshLocality(mesh,true));
    for (int pass=0;pass<4;++pass) {
        lv::MeshOptimizationOptions options;
        if (pass==0) {options.VertexFetch=false;}if (pass==2) {options.CoarsestLodFirst=false;}if (pass==3) {options.Overdraw=true;}
        const char* names[]={"cacheOnly","cacheFetchLodFirst","cacheFetchBaseFirst","cacheOverdrawFetch"};
        const auto start=std::chrono::steady_clock::now();lv::MeshOptimization result;for (int i=0;i<3;++i) {result=lv::OptimizeMesh(mesh,lods,options);}
        file<<",\""<<names[pass]<<"\":{\"optimizationMeanMs\":"<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/3<<",\"base\":";Stats(file,lv::AnalyzeMeshLocality(result.Geometry,true));
        if (!result.Lods.empty()) {auto coarse=result.Geometry;coarse.Indices=result.Lods.back().Indices;file<<",\"coarse\":";Stats(file,lv::AnalyzeMeshLocality(coarse));const auto [low,high]=std::minmax_element(coarse.Indices.begin(),coarse.Indices.end());file<<",\"coarseVertexRange\":"<<*high-*low+1;}
        file<<'}';
    }file<<'}';
}
}
int main(int argc,char** argv) {
    try {
        auto mesh=Grid(32,true);auto levels=lv::GenerateMeshLods(mesh);
        // Distinct vertex attributes and a LOD-only vertex must not be welded.
        for (std::size_t i=0;i<mesh.Vertices.size();++i) {mesh.Vertices[i].Joints={std::uint16_t(i%7),0,0,0};mesh.Vertices[i].Tangent={1,0,0,i%2 ? 1.0f:-1.0f};}
        mesh.Vertices.push_back(mesh.Vertices.front());levels.push_back({{0,1,static_cast<std::uint32_t>(mesh.Vertices.size()-1)},.01f});
        // OptimizeMesh supports three LODs; deliberately pass them out of size order.
        levels.erase(levels.begin());std::reverse(levels.begin(),levels.end());
        const auto optimized=lv::OptimizeMesh(mesh,levels);Preserves(mesh,levels,optimized);
        const auto coarse=std::min_element(optimized.Lods.begin(),optimized.Lods.end(),[](const auto& a,const auto& b){return a.Indices.size()<b.Indices.size();});
        std::set<unsigned> used(coarse->Indices.begin(),coarse->Indices.end());
        Check(*used.rbegin()+1==used.size(),"coarsest LOD occupies a compact vertex prefix even with LOD-only vertices");
        Preserves(mesh,levels,lv::OptimizeMesh(mesh,levels,{.Overdraw=true}));
        const auto unchanged=lv::OptimizeMesh(mesh,levels,{.VertexCache=false,.VertexFetch=false});
        Check(unchanged.Geometry.Indices==mesh.Indices&&unchanged.CacheMissesAfter==unchanged.CacheMissesBefore,"disabled passes preserve source triangle order and cache counts");
        Rejects([&]{(void)lv::OptimizeMesh(mesh,{}, {.VertexCache=false,.Overdraw=true});},"overdraw cannot run before vertex cache optimization");
        Rejects([&]{(void)lv::OptimizeMesh(mesh,{}, {.OverdrawThreshold=.9f});},"invalid overdraw threshold is rejected");
        const auto large=Grid(128,true);const auto raw=lv::AnalyzeMeshLocality(large);const auto cache=lv::OptimizeMesh(large,{}, {.VertexFetch=false});const auto fetched=lv::OptimizeMesh(large);
        const auto cacheStats=lv::AnalyzeMeshLocality(cache.Geometry),fetchStats=lv::AnalyzeMeshLocality(fetched.Geometry);
        Check(cacheStats.Acmr<raw.Acmr,"post-transform cache optimization reduces software ACMR on shuffled topology");
        Check(fetchStats.VertexBytesFetched<cacheStats.VertexBytesFetched,"vertex fetch ordering reduces modeled bytes on scattered vertex storage");
        Check(!raw.HasOverdraw&&lv::AnalyzeMeshLocality(mesh,true).HasOverdraw,"software overdraw analysis is explicit opt-in");
        const auto layered=Layers();const auto front=lv::OptimizeMesh(layered,{}, {.Overdraw=true});Preserves(layered,{},front);
        Check(lv::AnalyzeMeshLocality(front.Geometry,true).Overdraw<lv::AnalyzeMeshLocality(layered,true).Overdraw,"overdraw optimization reduces shaded software pixels on layered geometry");
        Check(lv::detail::IndexElementBytes(65536)==2&&lv::detail::IndexElementBytes(65537)==4,"index packing switches exactly at 65536 vertices");
        Check(lv::detail::IndexElementBytes(65535,true)==2&&lv::detail::IndexElementBytes(65536,true)==4,"WebGL2 reserves the 0xffff primitive restart index");
        const std::array<std::uint32_t,3> valid{0,1,65535};const auto packed=lv::detail::PackIndices16(valid);Check(std::equal(packed.begin(),packed.end(),valid.begin()),"16-bit packing preserves boundary indices");
        Rejects([]{const std::array<std::uint32_t,1> invalid{65536};(void)lv::detail::PackIndices16(invalid);},"16-bit overflow is rejected");
        if (argc==2) {std::ofstream file(argv[1]);file<<std::setprecision(12)<<"{\"format\":\"MinvexCacheEfficiency\",\"version\":1,\"statistics\":\"software models, not measured GPU performance\",\"cases\":[";Report(file,"scattered-grid",large,{});file<<',';const auto lodMesh=Grid(64,true);const auto lods=lv::GenerateMeshLods(lodMesh);Report(file,"lod-grid",lodMesh,lods);file<<',';Report(file,"layered-overdraw",Layers(),{});file<<"]}\n";file.close();if (!file) {throw std::runtime_error("Cannot save cache benchmark.");}}
    }catch(const std::exception& e) {Check(false,e.what());}
    return Finish();
}
