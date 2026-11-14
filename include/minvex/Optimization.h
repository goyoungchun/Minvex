#pragma once
#include <minvex/Mesh.h>
#include <minvex/Math.h>
#include <span>
#include <cstddef>
namespace lv {
struct MeshOptimization {
    MeshData Geometry;
    std::vector<MeshLod> Lods;
    std::uint64_t CacheMissesBefore=0,CacheMissesAfter=0; // Simulated 16-entry LRU, not hardware measurements.
    std::vector<std::uint32_t> VertexRemap; // Source vertex -> optimized vertex, including unused vertices.
};
// Reorders triangles/vertices without changing attributes or source data. Retains unused vertices.
MeshOptimization OptimizeMesh(const MeshData& mesh,std::span<const MeshLod> lods={});
struct MeshOptimizationOptions {
    bool VertexCache=true,VertexFetch=true,CoarsestLodFirst=true;
    bool Overdraw=false; // Opaque/static content only; caller owns this decision.
    float OverdrawThreshold=1.05f; // Allowed cache tradeoff; finite [1,3].
};
MeshOptimization OptimizeMesh(const MeshData& mesh,std::span<const MeshLod> lods,const MeshOptimizationOptions& options);
struct MeshLocalityStatistics {
    std::uint64_t VerticesTransformed=0,VertexBytesFetched=0,PixelsCovered=0,PixelsShaded=0;
    double Acmr=0,Atvr=0,Overfetch=0,Overdraw=0;
    bool HasOverdraw=false;
};
// Software cache/fetch/raster models, never measured GPU timings. Overdraw is opt-in.
MeshLocalityStatistics AnalyzeMeshLocality(const MeshData& mesh,bool overdraw=false);
// Conservative attribute-aware index-only LOD1..3, target ratios .5/.25/.125; may stop earlier.
std::vector<MeshLod> GenerateMeshLods(const MeshData& mesh,float targetError=.02f);
unsigned SelectMeshLod(float projectedDiameterPixels,float bias,std::size_t reducedLevels);
std::uint64_t TextureMipBytes(int width,int height,bool bc3=false);
struct OptimizationStatistics {
    std::uint64_t InstancedObjects=0,InstanceDraws=0,SavedDrawCalls=0,InstanceBufferBytes=0;
    std::array<std::uint64_t,4> LodDraws{};
    std::uint64_t SourceTriangles=0,SubmittedTriangles=0,CulledParts=0;
};
}
