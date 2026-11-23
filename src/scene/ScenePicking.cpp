#include <minvex/Scene.h>

#include <cmath>
#include <stdexcept>

namespace lv {
namespace {
struct Vector {
    double X, Y, Z;
    Vector operator+(Vector b) const { return {X+b.X, Y+b.Y, Z+b.Z}; }
    Vector operator-(Vector b) const { return {X-b.X, Y-b.Y, Z-b.Z}; }
    Vector operator*(double s) const { return {X*s, Y*s, Z*s}; }
    double Dot(Vector b) const { return X*b.X + Y*b.Y + Z*b.Z; }
    Vector Cross(Vector b) const { return {Y*b.Z-Z*b.Y, Z*b.X-X*b.Z, X*b.Y-Y*b.X}; }
    double Length() const { return std::hypot(X,Y,Z); }
    bool Finite() const { return std::isfinite(X) && std::isfinite(Y) && std::isfinite(Z); }
    Vec3 ToVec3() const {
        Vec3 result{static_cast<float>(X),static_cast<float>(Y),static_cast<float>(Z)};
        if (!std::isfinite(result.X) || !std::isfinite(result.Y) || !std::isfinite(result.Z)) {
            throw std::invalid_argument("Coordinate exceeds the supported float range.");
        }
        return result;
    }
};
Vector ToVector(Vec3 p) { return {p.X,p.Y,p.Z}; }

struct ScreenCamera {
    Mat4 Projection;
    Vector Eye, Right, Up, Back;
    bool Perspective;

    ScreenCamera(const Camera& camera, ViewportSize size)
        : Eye(ToVector(camera.Position)), Perspective(camera.Projection == CameraProjection::Perspective) {
        if (!std::isfinite(size.Width) || !std::isfinite(size.Height) || size.Width <= 0 || size.Height <= 0) {
            throw std::invalid_argument("Screen coordinates require a positive finite viewport.");
        }
        Projection = camera.ProjectionMatrix(size.Width / size.Height);
        const auto view = Mat4::LookAt(camera.Position,camera.Target,camera.Up).Elements;
        Right = {view[0],view[4],view[8]};
        Up = {view[1],view[5],view[9]};
        Back = {view[2],view[6],view[10]};
    }

    Vector Offset(ViewportSize size, float x, float y) const {
        if (!std::isfinite(x) || !std::isfinite(y)) { throw std::invalid_argument("Screen coordinates must be finite."); }
        return Right*((2.0*x/size.Width-1)/Projection.Elements[0]) +
               Up*((1-2.0*y/size.Height)/Projection.Elements[5]);
    }
};
} // namespace

ScreenPoint WorldToScreen(const Camera& camera, ViewportSize viewport, Vec3 world) {
    const ScreenCamera view(camera,viewport);
    const auto relative = ToVector(world)-view.Eye;
    const double z = relative.Dot(view.Back);
    if (view.Perspective && z >= 0) { throw std::invalid_argument("Point must be in front of the perspective camera."); }
    const auto& m = view.Projection.Elements;
    const double w = view.Perspective ? -z : 1;
    const double near = camera.NearPlane, far = camera.FarPlane;
    const double depth = view.Perspective ? far*(-z-near)/(w*(far-near)) : (-z-near)/(far-near);
    const auto point = Vector{(1+relative.Dot(view.Right)*m[0]/w)*viewport.Width/2,
                              (1-relative.Dot(view.Up)*m[5]/w)*viewport.Height/2,0}.ToVec3();
    if (!std::isfinite(depth)) { throw std::invalid_argument("Projected depth must be finite."); }
    return {point.X,point.Y,depth};
}

Vec3 ScreenToWorld(const Camera& camera, ViewportSize viewport, ScreenPoint point) {
    if (!std::isfinite(point.Depth) || point.Depth < 0 || point.Depth > 1) {
        throw std::invalid_argument("Screen depth must be in [0, 1].");
    }
    const ScreenCamera view(camera,viewport);
    const auto offset = view.Offset(viewport,point.X,point.Y);
    // Use the camera planes to avoid precision loss when a perspective matrix
    // rounds its far depth coefficient to -1.
    const double near = camera.NearPlane, far = camera.FarPlane;
    const double depth = view.Perspective ? near*far/(far*(1.0-point.Depth)+near*point.Depth)
                                          : near+(far-near)*point.Depth;
    return (view.Eye - view.Back*depth + offset*(view.Perspective ? depth : 1)).ToVec3();
}

Ray ScreenRay(const Camera& camera, ViewportSize viewport, float x, float y) {
    const ScreenCamera view(camera,viewport);
    const auto offset = view.Offset(viewport,x,y);
    const auto direction = view.Perspective ? offset-view.Back : view.Back*-1;
    const double length = direction.Length();
    return {(view.Perspective ? view.Eye : view.Eye+offset).ToVec3(), (direction*(1/length)).ToVec3(),
            camera.NearPlane*length, camera.FarPlane*length};
}

std::optional<SurfaceHit> Scene::PickSurface(const Ray& ray, ObjectId object) const {
    const auto origin = ToVector(ray.Origin);
    const auto heading = ToVector(ray.Direction);
    const auto length = heading.Length();
    if (!origin.Finite() || !std::isfinite(length) || length <= 0 || !std::isfinite(ray.MinDistance) || ray.MinDistance < 0 ||
        std::isnan(ray.MaxDistance) || ray.MaxDistance < ray.MinDistance) {
        throw std::invalid_argument("Invalid surface ray or distance interval.");
    }
    const auto direction = heading*(1/length);
    std::optional<SurfaceHit> hit;
    double closest = ray.MaxDistance;
    for (const auto& item : CollectRenderItems()) {
        if (object != NoObject && item.Object != object) { continue; }
        for (std::size_t partIndex=0; partIndex<item.Model->Parts.size(); ++partIndex) {
            const auto& part = item.Model->Parts[partIndex];
            const auto& mesh = item.Model->Meshes.at(part.MeshIndex);
            const auto weights = mesh.MorphWeights.empty() ? std::vector<float>{} : item.Animation->EvaluateMorphWeights(part.Node);
            auto transform = item.WorldTransform * (item.Pose && part.Node != NoJoint ? item.Pose->Global.at(part.Node) : part.Transform);
            std::vector<Mat4> palette;
            if (part.SkinIndex) {
                if (!item.Pose) { throw std::logic_error("Skinned instances require evaluated pose state."); }
                const auto& skin = item.Model->Skins.at(*part.SkinIndex);
                for (std::size_t joint=0; joint<skin.Joints.size(); ++joint) {
                    palette.push_back(item.Pose->Global.at(skin.Joints[joint])*skin.InverseBindMatrices.at(joint));
                }
                transform = item.WorldTransform;
            }
            for (std::size_t primitiveIndex=0; primitiveIndex<mesh.Primitives.size(); ++primitiveIndex) {
                const auto& primitive = mesh.Primitives[primitiveIndex];
                const auto morph = weights.empty() ? MeshData{} : EvaluateMorph(primitive,weights);
                const auto& geometry = weights.empty() ? primitive.Geometry : morph;
                if (geometry.Indices.size()%3 != 0) { throw std::invalid_argument("Surface picking requires a triangle list."); }
                std::vector<Vector> points;
                points.reserve(geometry.Vertices.size());
                for (const auto& vertex : geometry.Vertices) {
                    Vec3 position{vertex.Position[0],vertex.Position[1],vertex.Position[2]};
                    if (!palette.empty()) {
                        Vector skinned{};
                        double weightSum = 0;
                        for (std::size_t j=0; j<vertex.Weights.size(); ++j) {
                            if (vertex.Weights[j] != 0) {
                                skinned = skinned + ToVector(palette.at(vertex.Joints[j]).TransformPoint(position))*vertex.Weights[j];
                                weightSum += vertex.Weights[j];
                            }
                        }
                        position = (skinned*(1/weightSum)).ToVec3();
                    }
                    const auto world = ToVector(transform.TransformPoint(position));
                    if (!world.Finite()) { throw std::invalid_argument("Surface vertex exceeds the supported coordinate range."); }
                    points.push_back(world);
                }
                for (std::size_t index=0; index<geometry.Indices.size(); index+=3) {
                    const auto a = points.at(geometry.Indices[index]);
                    const auto ab = points.at(geometry.Indices[index+1])-a;
                    const auto ac = points.at(geometry.Indices[index+2])-a;
                    const auto p = direction.Cross(ac);
                    const double determinant = ab.Dot(p);
                    if (std::abs(determinant) <= 1e-14*ab.Length()*p.Length()) { continue; }
                    const auto fromA = origin-a;
                    const auto q = fromA.Cross(ab);
                    const double u = fromA.Dot(p)/determinant;
                    const double v = direction.Dot(q)/determinant;
                    const double distance = ac.Dot(q)/determinant;
                    constexpr double edgeTolerance = 1e-12;
                    if (u < -edgeTolerance || v < -edgeTolerance || u+v > 1+edgeTolerance ||
                        !std::isfinite(distance) || distance < ray.MinDistance || distance > closest ||
                        (hit && distance == closest)) { continue; }
                    closest = distance;
                    hit = SurfaceHit{item.Object,(origin+direction*distance).ToVec3(),distance,
                                     partIndex,primitiveIndex,index/3,{1-u-v,u,v}};
                }
            }
        }
    }
    return hit;
}
} // namespace lv
