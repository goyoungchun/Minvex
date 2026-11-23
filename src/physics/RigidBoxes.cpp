#include <minvex/Physics.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace lv {
namespace {
constexpr double ContactMargin = 0.0005;
constexpr double PenetrationSlop = 0.0002;
constexpr int SolverIterations = 24;

struct Vector {
    double X = 0, Y = 0, Z = 0;
    Vector operator+(Vector b) const { return {X+b.X,Y+b.Y,Z+b.Z}; }
    Vector operator-(Vector b) const { return {X-b.X,Y-b.Y,Z-b.Z}; }
    Vector operator*(double scale) const { return {X*scale,Y*scale,Z*scale}; }
    double Dot(Vector b) const { return X*b.X+Y*b.Y+Z*b.Z; }
    Vector Cross(Vector b) const { return {Y*b.Z-Z*b.Y,Z*b.X-X*b.Z,X*b.Y-Y*b.X}; }
};
Vector Read(Vec3 v) { return {v.X,v.Y,v.Z}; }
bool Finite(Vector v) { return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z); }
Vec3 Write(Vector v) {
    constexpr double limit = std::numeric_limits<float>::max();
    if (!Finite(v) || std::max({std::abs(v.X),std::abs(v.Y),std::abs(v.Z)}) > limit) {
        throw std::overflow_error("Rigid box state exceeds the float range.");
    }
    return {static_cast<float>(v.X),static_cast<float>(v.Y),static_cast<float>(v.Z)};
}

struct Orientation {
    Vector Imaginary;
    double Real;

    void Normalize() {
        const double length = std::sqrt(Imaginary.Dot(Imaginary)+Real*Real);
        if (!std::isfinite(length) || length == 0) { throw std::invalid_argument("Rigid box rotation must be finite and nonzero."); }
        Imaginary = Imaginary*(1/length);
        Real /= length;
    }
    Vector Rotate(Vector v) const {
        const auto twiceCross = Imaginary.Cross(v)*2;
        return v+twiceCross*Real+Imaginary.Cross(twiceCross);
    }
    void Advance(Vector spin,double dt) {
        const double speed = std::sqrt(spin.Dot(spin));
        const double angle = speed*dt*0.5;
        const double real = std::cos(angle);
        const auto imaginary = spin*(speed > 0 ? std::sin(angle)/speed : 0);
        const double newReal = real*Real-imaginary.Dot(Imaginary);
        Imaginary = Imaginary*real+imaginary*Real+imaginary.Cross(Imaginary);
        Real = newReal;
        Normalize();
    }
};

struct Body {
    Vector Position, Velocity, Spin;
    Vector PositionCorrection{}, RotationCorrection{};
    Orientation Rotation;
    std::array<double,3> Half, Inertia{}, InvInertia{};
    std::array<Vector,3> Axes;
    Vector Range;
    double InvMass, Friction;

    explicit Body(const RigidBox& box) : Position(Read(box.Position)), Velocity(Read(box.LinearVelocity)),
        Spin(Read(box.AngularVelocity)), Rotation{{box.Rotation.X,box.Rotation.Y,box.Rotation.Z},box.Rotation.W},
        Half{box.HalfExtents.X,box.HalfExtents.Y,box.HalfExtents.Z},
        InvMass(box.Mass > 0 ? 1.0/box.Mass : 0), Friction(box.Friction) {
        if (!Finite(Position) || !Finite(Velocity) || !Finite(Spin) || !std::isfinite(box.Mass) || box.Mass < 0 ||
            !std::isfinite(Friction) || Friction < 0 || std::ranges::any_of(Half,[](double half) { return !std::isfinite(half) || half <= 0; })) {
            throw std::invalid_argument("Rigid boxes require finite state, positive half-extents, and nonnegative mass and friction.");
        }
        Rotation.Normalize();
        if (InvMass > 0) {
            for (std::size_t i=0; i<3; ++i) {
                const double a=Half[(i+1)%3], b=Half[(i+2)%3];
                Inertia[i] = box.Mass*(a*a+b*b)/3;
                InvInertia[i] = 1/Inertia[i];
            }
        } else { Velocity = {}; Spin = {}; }
        UpdateAxes();
    }

    void UpdateAxes() {
        Axes = {Rotation.Rotate({1,0,0}),Rotation.Rotate({0,1,0}),Rotation.Rotate({0,0,1})};
        Range = {Radius({1,0,0}),Radius({0,1,0}),Radius({0,0,1})};
    }
    double Radius(Vector normal) const {
        return std::abs(normal.Dot(Axes[0]))*Half[0]+std::abs(normal.Dot(Axes[1]))*Half[1]+std::abs(normal.Dot(Axes[2]))*Half[2];
    }
    Vector MultiplyInertia(Vector v,const std::array<double,3>& diagonal) const {
        return Axes[0]*(Axes[0].Dot(v)*diagonal[0])+Axes[1]*(Axes[1].Dot(v)*diagonal[1])+Axes[2]*(Axes[2].Dot(v)*diagonal[2]);
    }
    void Advance(Vector gravity,double dt) {
        if (InvMass == 0) { return; }
        Velocity = Velocity+gravity*dt;
        Position = Position+Velocity*dt;
        Rotate(Spin,dt);
    }
    void Rotate(Vector spin,double dt) {
        const auto momentum = MultiplyInertia(Spin,Inertia);
        Rotation.Advance(spin,dt);
        UpdateAxes();
        Spin = MultiplyInertia(momentum,InvInertia);
    }
    void Impulse(Vector impulse,Vector arm,Vector& velocity,Vector& spin) const {
        if (InvMass == 0) { return; }
        velocity = velocity+impulse*InvMass;
        spin = spin+MultiplyInertia(arm.Cross(impulse),InvInertia);
    }
    void CorrectPose(double dt) {
        Position = Position+PositionCorrection*dt;
        if (RotationCorrection.Dot(RotationCorrection) > 0) { Rotate(RotationCorrection,dt); }
        PositionCorrection = {};
        RotationCorrection = {};
    }
};

enum class Feature { FirstFace, SecondFace, Edges };
struct AxisHit {
    Vector Normal;
    double Depth;
    Feature Type;
    std::size_t FirstAxis, SecondAxis;
};

std::optional<AxisHit> Intersect(const Body& a,const Body& b) {
    const auto delta = b.Position-a.Position;
    const auto range = a.Range+b.Range;
    if (std::abs(delta.X) > range.X+ContactMargin || std::abs(delta.Y) > range.Y+ContactMargin ||
        std::abs(delta.Z) > range.Z+ContactMargin) { return std::nullopt; }
    AxisHit best{{},std::numeric_limits<double>::infinity(),Feature::FirstFace,0,0};
    const auto axis = [&](Vector n,Feature type,std::size_t i,std::size_t j) {
        const double direction = delta.Dot(n);
        const double overlap = a.Radius(n)+b.Radius(n)-std::abs(direction);
        if (overlap < -ContactMargin) { return false; }
        // A face gives a stable manifold when a nearly parallel edge gives
        // effectively the same separating direction.
        const double tie = type == Feature::Edges && best.Type != Feature::Edges ? PenetrationSlop*0.25 : 0;
        if (overlap+tie < best.Depth) { best = {direction >= 0 ? n : n*-1,overlap,type,i,j}; }
        return true;
    };
    for (std::size_t i=0; i<3; ++i) {
        if (!axis(a.Axes[i],Feature::FirstFace,i,0) || !axis(b.Axes[i],Feature::SecondFace,0,i)) { return std::nullopt; }
    }
    for (std::size_t i=0; i<3; ++i) {
        for (std::size_t j=0; j<3; ++j) {
            const auto n = a.Axes[i].Cross(b.Axes[j]);
            const double squared = n.Dot(n);
            if (squared > 1e-10 && !axis(n*(1/std::sqrt(squared)),Feature::Edges,i,j)) { return std::nullopt; }
        }
    }
    return best;
}

struct Polygon { std::array<Vector,8> Points{}; std::size_t Count = 0; };
Polygon Clip(const Polygon& input,Vector origin,Vector normal,double extent) {
    if (input.Count == 0) { return {}; }
    Polygon result;
    auto previous = input.Points[input.Count-1];
    double last = normal.Dot(previous-origin)-extent;
    for (std::size_t i=0; i<input.Count; ++i) {
        const auto point = input.Points[i];
        const double distance = normal.Dot(point-origin)-extent;
        if ((last < 0 && distance > 0) || (last > 0 && distance < 0)) {
            result.Points.at(result.Count++) = previous+(point-previous)*(last/(last-distance));
        }
        if (distance <= 0) { result.Points.at(result.Count++) = point; }
        previous = point;
        last = distance;
    }
    return result;
}

Vector EdgeCenter(const Body& body,std::size_t edge,Vector normal) {
    auto center = body.Position;
    for (std::size_t offset=1; offset<=2; ++offset) {
        const auto i = (edge+offset)%3;
        center = center+body.Axes[i]*(normal.Dot(body.Axes[i]) >= 0 ? body.Half[i] : -body.Half[i]);
    }
    return center;
}

Vector EdgePoint(const Body& a,const Body& b,const AxisHit& hit) {
    const auto u = a.Axes[hit.FirstAxis], v = b.Axes[hit.SecondAxis];
    const auto ca = EdgeCenter(a,hit.FirstAxis,hit.Normal), cb = EdgeCenter(b,hit.SecondAxis,hit.Normal*-1);
    const auto d = ca-cb;
    const double ha = a.Half[hit.FirstAxis], hb = b.Half[hit.SecondAxis];
    const double uv = u.Dot(v), du = d.Dot(u), dv = d.Dot(v);
    auto s = std::clamp((uv*dv-du)/(1-uv*uv),-ha,ha);
    const auto t = std::clamp(dv+uv*s,-hb,hb);
    s = std::clamp(uv*t-du,-ha,ha);
    return (ca+u*s+cb+v*t)*0.5;
}

struct Contact {
    std::size_t First, Second;
    Vector Position, Normal;
    double Depth;
    Vector ArmA{}, ArmB{}, Tangent1{}, Tangent2{};
    double NormalMass = 0, TangentMass11 = 0, TangentMass12 = 0, TangentMass22 = 0;
    double ApproachSpeed = 0, SeparationSpeed = 0, Friction = 0;
    double ImpulseN = 0, ImpulseT1 = 0, ImpulseT2 = 0, CorrectionImpulse = 0;
};

void AddContacts(std::vector<Contact>& contacts,const std::vector<Body>& bodies,std::size_t i,std::size_t j,const AxisHit& hit) {
    const auto& a = bodies[i];
    const auto& b = bodies[j];
    if (hit.Type == Feature::Edges) {
        contacts.push_back({i,j,EdgePoint(a,b,hit),hit.Normal,hit.Depth});
        return;
    }
    const bool first = hit.Type == Feature::FirstFace;
    const auto& reference = first ? a : b;
    const auto& incident = first ? b : a;
    const auto refAxis = first ? hit.FirstAxis : hit.SecondAxis;
    const auto normal = first ? hit.Normal : hit.Normal*-1;
    const auto refCenter = reference.Position+normal*reference.Half[refAxis];
    std::size_t face = 0;
    for (std::size_t axis=1; axis<3; ++axis) {
        if (std::abs(normal.Dot(incident.Axes[axis])) > std::abs(normal.Dot(incident.Axes[face]))) { face = axis; }
    }
    const auto center = incident.Position+incident.Axes[face]*(normal.Dot(incident.Axes[face]) > 0 ? -incident.Half[face] : incident.Half[face]);
    const auto u = incident.Axes[(face+1)%3]*incident.Half[(face+1)%3];
    const auto v = incident.Axes[(face+2)%3]*incident.Half[(face+2)%3];
    Polygon polygon{{center-u-v,center+u-v,center+u+v,center-u+v},4};
    for (std::size_t offset=1; offset<=2; ++offset) {
        const auto k = (refAxis+offset)%3;
        polygon = Clip(polygon,refCenter,reference.Axes[k],reference.Half[k]);
        polygon = Clip(polygon,refCenter,reference.Axes[k]*-1,reference.Half[k]);
    }
    for (std::size_t k=0; k<polygon.Count; ++k) {
        const auto point = polygon.Points[k];
        const double distance = normal.Dot(point-refCenter);
        if (distance <= ContactMargin) { contacts.push_back({i,j,point-normal*(distance*0.5),hit.Normal,-distance}); }
    }
}

double InverseEffectiveMass(const Body& a,const Body& b,Vector armA,Vector armB,Vector u,Vector v) {
    return (a.InvMass+b.InvMass)*u.Dot(v)+armA.Cross(u).Dot(a.MultiplyInertia(armA.Cross(v),a.InvInertia))+
           armB.Cross(u).Dot(b.MultiplyInertia(armB.Cross(v),b.InvInertia));
}

void Prepare(Contact& contact,const Body& a,const Body& b,double dt) {
    contact.ArmA = contact.Position-a.Position;
    contact.ArmB = contact.Position-b.Position;
    const auto n = contact.Normal;
    const auto tangent = n.Cross(std::abs(n.X) < 0.707 ? Vector{1,0,0} : Vector{0,1,0});
    contact.Tangent1 = tangent*(1/std::sqrt(tangent.Dot(tangent)));
    contact.Tangent2 = n.Cross(contact.Tangent1);
    const auto mass = [&](Vector u,Vector v) { return InverseEffectiveMass(a,b,contact.ArmA,contact.ArmB,u,v); };
    contact.NormalMass = 1/mass(n,n);
    const double m11 = mass(contact.Tangent1,contact.Tangent1);
    const double m12 = mass(contact.Tangent1,contact.Tangent2);
    const double m22 = mass(contact.Tangent2,contact.Tangent2);
    const double determinant = m11*m22-m12*m12;
    if (determinant > 0) {
        contact.TangentMass11 = m22/determinant;
        contact.TangentMass12 = -m12/determinant;
        contact.TangentMass22 = m11/determinant;
    }
    contact.ApproachSpeed = std::min(0.0,contact.Depth)/dt;
    contact.SeparationSpeed = std::clamp((contact.Depth-PenetrationSlop)*0.18/dt,0.0,2.0);
    contact.Friction = std::sqrt(a.Friction*b.Friction);
}

void Solve(Contact& c,Body& a,Body& b) {
    const auto relative = [&] { return b.Velocity+b.Spin.Cross(c.ArmB)-a.Velocity-a.Spin.Cross(c.ArmA); };
    const auto apply = [&](Vector impulse) {
        a.Impulse(impulse*-1,c.ArmA,a.Velocity,a.Spin);
        b.Impulse(impulse,c.ArmB,b.Velocity,b.Spin);
    };
    const double oldNormal = c.ImpulseN;
    c.ImpulseN = std::max(0.0,oldNormal+(c.ApproachSpeed-relative().Dot(c.Normal))*c.NormalMass);
    apply(c.Normal*(c.ImpulseN-oldNormal));
    if (c.Friction == 0) { return; }
    const auto slip = relative();
    const double v1 = slip.Dot(c.Tangent1), v2 = slip.Dot(c.Tangent2);
    double t1 = c.ImpulseT1-c.TangentMass11*v1-c.TangentMass12*v2;
    double t2 = c.ImpulseT2-c.TangentMass12*v1-c.TangentMass22*v2;
    const double limit = c.Friction*c.ImpulseN;
    const double length = std::hypot(t1,t2);
    if (length > limit) { t1 *= limit/length; t2 *= limit/length; }
    apply(c.Tangent1*(t1-c.ImpulseT1)+c.Tangent2*(t2-c.ImpulseT2));
    c.ImpulseT1 = t1;
    c.ImpulseT2 = t2;
}

void SolvePosition(Contact& c,Body& a,Body& b) {
    if (c.Depth < 0) { return; }
    const auto relative = b.PositionCorrection+b.RotationCorrection.Cross(c.ArmB)-
                          a.PositionCorrection-a.RotationCorrection.Cross(c.ArmA);
    const auto previous = c.CorrectionImpulse;
    c.CorrectionImpulse = std::max(0.0,previous+(c.SeparationSpeed-relative.Dot(c.Normal))*c.NormalMass);
    const auto impulse = c.Normal*(c.CorrectionImpulse-previous);
    a.Impulse(impulse*-1,c.ArmA,a.PositionCorrection,a.RotationCorrection);
    b.Impulse(impulse,c.ArmB,b.PositionCorrection,b.RotationCorrection);
}
} // namespace

RigidBoxStep StepRigidBoxes(std::span<const RigidBox> boxes,RigidBoxStepOptions options) {
    if (boxes.size() > MaxPhysicsBoxes) { throw std::length_error("Rigid box physics supports up to 256 colliders."); }
    const auto gravity = Read(options.Gravity);
    if (!std::isfinite(options.Seconds) || options.Seconds < 0 || options.Seconds > 1 || !Finite(gravity)) {
        throw std::invalid_argument("A rigid box step needs finite gravity and a duration in [0,1] seconds.");
    }
    std::vector<Body> bodies;
    bodies.reserve(boxes.size());
    for (const auto& box : boxes) { bodies.emplace_back(box); }
    const int count = std::max(1,static_cast<int>(std::ceil(options.Seconds*240)));
    const double dt = options.Seconds/count;
    std::vector<Contact> contacts;
    for (int step=0; step<count; ++step) {
        if (dt > 0) { for (auto& body : bodies) { body.Advance(gravity,dt); } }
        contacts.clear();
        for (std::size_t i=0; i<bodies.size(); ++i) {
            for (std::size_t j=i+1; j<bodies.size(); ++j) {
                if (bodies[i].InvMass == 0 && bodies[j].InvMass == 0) { continue; }
                if (const auto hit = Intersect(bodies[i],bodies[j])) { AddContacts(contacts,bodies,i,j,*hit); }
            }
        }
        if (dt == 0) { break; }
        for (auto& c : contacts) { Prepare(c,bodies[c.First],bodies[c.Second],dt); }
        for (int iteration=0; iteration<SolverIterations; ++iteration) {
            for (auto& c : contacts) {
                auto& a = bodies[c.First];
                auto& b = bodies[c.Second];
                Solve(c,a,b);
                SolvePosition(c,a,b);
            }
        }
        for (auto& body : bodies) { body.CorrectPose(dt); }
    }

    RigidBoxStep result{{boxes.begin(),boxes.end()}, {}};
    for (std::size_t i=0; i<bodies.size(); ++i) {
        const auto& body = bodies[i];
        if (dt == 0 || body.InvMass == 0) { continue; }
        auto& box = result.Boxes[i];
        box.Position = Write(body.Position);
        box.LinearVelocity = Write(body.Velocity);
        box.AngularVelocity = Write(body.Spin);
        const auto q = Write(body.Rotation.Imaginary);
        box.Rotation = {q.X,q.Y,q.Z,static_cast<float>(body.Rotation.Real)};
    }
    result.Contacts.reserve(contacts.size());
    for (const auto& c : contacts) {
        if (c.Depth < 0 && c.ImpulseN == 0) { continue; }
        result.Contacts.push_back({c.First,c.Second,Write(c.Position),Write(c.Normal),Write({std::max(0.0,c.Depth),0,0}).X});
    }
    return result;
}
} // namespace lv
