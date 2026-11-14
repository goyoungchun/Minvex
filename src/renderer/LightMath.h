#pragma once
#include <minvex/Scene.h>
#include <algorithm>
#include <cmath>
namespace lv::detail {
inline Vec3 LightCross(Vec3 a,Vec3 b) {return {a.Y*b.Z-a.Z*b.Y,a.Z*b.X-a.X*b.Z,a.X*b.Y-a.Y*b.X};}
inline Vec3 LightUnit(Vec3 v) {const auto n=std::hypot(double(v.X),double(v.Y),double(v.Z));return {static_cast<float>(v.X/n),static_cast<float>(v.Y/n),static_cast<float>(v.Z/n)};}
inline std::array<Vec3,2> RectAxes(const RectLight& l) {auto n=LightUnit(l.Direction),r=LightUnit(LightCross(LightUnit(l.Up),n));return {r,LightCross(n,r)};}
inline Mat4 PointShadowMatrix(const PointLight& l,unsigned face) {
    constexpr Vec3 directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    constexpr Vec3 ups[]={{0,1,0},{0,1,0},{0,0,-1},{0,0,1},{0,1,0},{0,1,0}};
    if (face>=6) {throw std::invalid_argument("Invalid point shadow face.");}const auto d=directions[face];
    return Mat4::Perspective(1.570796326795f,1,l.Shadow.NearPlane,l.Range)*Mat4::LookAt({},d,ups[face])*Mat4::Translation({-l.Position.X,-l.Position.Y,-l.Position.Z});
}
inline Mat4 SpotShadowMatrix(const SpotLight& l) {const auto d=LightUnit(l.Direction);return Mat4::Perspective(l.OuterConeRadians*2,1,l.Shadow.NearPlane,l.Range)*Mat4::LookAt({},d,std::abs(d.Y)>.98f ? Vec3{0,0,1}:Vec3{0,1,0})*Mat4::Translation({-l.Position.X,-l.Position.Y,-l.Position.Z});}
struct LocalShadowLayout {int Faces=0,Columns=0,Rows=0,Resolution=0;int Width() const {return Columns*Resolution;}int Height() const {return Rows*Resolution;}std::uint64_t Bytes() const {return std::uint64_t(Width())*Height()*8;}};
inline LocalShadowLayout ShadowLayout(const Scene& scene) {LocalShadowLayout r;r.Resolution=scene.LocalShadowResolution;for (const auto& l:scene.PointLights) {if(l.Shadow.Enabled&&l.Intensity>0){r.Faces+=6;}}for (const auto& l:scene.SpotLights) {if(l.Shadow.Enabled&&l.Intensity>0){++r.Faces;}}r.Columns=std::min(6,r.Faces);r.Rows=r.Columns ? (r.Faces+r.Columns-1)/r.Columns:0;return r;}
inline void HashLightAllocations(std::uint64_t& hash,const Scene& scene,bool advanced,bool shadows) {const auto add=[&](unsigned value){hash^=value;hash*=1099511628211ull;};add(advanced&&!scene.RectLights.empty());const auto l=advanced&&shadows ? ShadowLayout(scene):LocalShadowLayout{};add(l.Faces);add(l.Faces ? l.Resolution:0);}
}
