#include <minvex/Physics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool condition,const char* message) { if (!condition) { throw std::runtime_error(message); } }

lv::RigidBox Box(lv::Vec3 center,lv::Vec3 half,float mass,float friction = 0.55f) {
    lv::RigidBox box;
    box.Position = center;
    box.HalfExtents = half;
    box.Mass = mass;
    box.Friction = friction;
    return box;
}

void Overhang() {
    const std::array plan{Box({0,-0.15f,0},{3,0.15f,1.7f},0),Box({0,0.35f,0},{1.6f,0.35f,1.25f},0),
        Box({1.84f,1.23f,0.15f},{0.5f,0.5f,0.4f},2),Box({-0.85f,1.15f,-0.2f},{0.55f,0.45f,0.45f},2)};
    auto boxes = std::vector(plan.begin(),plan.end());
    bool touchedPallet = false, tilted = false, fell = false, touchedDeck = false;
    for (int frame=0; frame<150; ++frame) {
        auto step = lv::StepRigidBoxes(boxes);
        const auto& cargo = step.Boxes[2];
        for (const auto& contact : step.Contacts) {
            if (contact.Second != 2) { continue; }
            touchedPallet |= contact.First == 1;
            touchedDeck |= contact.First == 0;
        }
        tilted |= touchedPallet && std::abs(cargo.Rotation.Z) > 0.18f;
        fell |= tilted && cargo.Position.X > 2 && cargo.Position.Y < 0.6f;
        boxes = std::move(step.Boxes);
    }
    Check(touchedPallet && tilted && fell && touchedDeck,"overhanging box must tip against the pallet and fall to the deck");
    std::printf("Final cargo center %.3f %.3f %.3f, orientation %.3f %.3f %.3f %.3f, vertical speed %.3f\n",
        boxes[2].Position.X,boxes[2].Position.Y,boxes[2].Position.Z,boxes[2].Rotation.X,boxes[2].Rotation.Y,
        boxes[2].Rotation.Z,boxes[2].Rotation.W,boxes[2].LinearVelocity.Y);
    Check(std::abs(boxes[2].LinearVelocity.Y)<0.02f && std::abs(boxes[2].Rotation.Z)>0.5f,
          "fallen box settles with a changed orientation");
    Check(std::abs(boxes[3].Position.Y-plan[3].Position.Y)<0.003f && std::abs(boxes[3].Rotation.Z)<0.002f,
          "centered cargo remains supported and upright");
    Check(plan[2].Position.X==1.84f && plan[2].Position.Y==1.23f && plan[2].Rotation.W==1,
          "preview steps must preserve the input plan");
}

float Slide(float friction) {
    auto cargo = Box({0,0.5f,0},{0.5f,0.5f,0.5f},3,friction);
    cargo.LinearVelocity.X = 2;
    const std::array boxes{Box({0,-0.2f,0},{8,0.2f,8},0,friction),cargo};
    const auto result = lv::StepRigidBoxes(boxes,{0.5});
    Check(result.Boxes[1].Position.Y > 0.495f,"ground contact must limit vertical penetration");
    return result.Boxes[1].LinearVelocity.X;
}

void DynamicContact() {
    Check(std::abs(Slide(0)-2)<0.001f && std::abs(Slide(0.8f))<0.2f,
          "Coulomb friction should slow a sliding box while frictionless contact preserves speed");
    auto first = Box({-0.65f,0,0},{0.5f,0.5f,0.5f},1,0);
    first.LinearVelocity.X = 3;
    const std::array boxes{first,Box({0.65f,0,0},{0.5f,0.5f,0.5f},2,0)};
    const auto step = lv::StepRigidBoxes(boxes,{0.2,{}});
    const auto& a = step.Boxes[0];
    const auto& b = step.Boxes[1];
    Check(b.LinearVelocity.X > 0.9f && std::abs(a.LinearVelocity.X-b.LinearVelocity.X)<0.2f &&
          std::abs(a.LinearVelocity.X+2*b.LinearVelocity.X-3)<0.001f,"dynamic contacts transfer normal momentum to both masses");
    Check(boxes[0].LinearVelocity.X == 3 && boxes[1].LinearVelocity.X == 0,"contact solver returns a copy");

    const std::array stack{Box({0,-0.15f,0},{3,0.15f,3},0),Box({0,0.5f,0},{0.5f,0.5f,0.5f},2),
        Box({0.1f,1.53f,0},{0.4f,0.5f,0.4f},1)};
    const auto resting = lv::StepRigidBoxes(stack,{0.75}).Boxes;
    Check(std::abs(resting[2].Position.Y-1.5f)<0.005f && std::abs(resting[2].LinearVelocity.Y)<0.02f &&
          std::abs(resting[1].Position.Y-0.5f)<0.003f,"dynamic boxes support a resting load without sinking");
}

void RotationAndValidation() {
    auto box = Box({},{0.4f,0.5f,0.6f},2);
    box.AngularVelocity.Z = 2;
    const std::array boxes{box};
    const auto a = lv::StepRigidBoxes(boxes,{0.25,{}}).Boxes[0];
    const auto b = lv::StepRigidBoxes(boxes,{0.25,{}}).Boxes[0];
    Check(std::abs(a.Rotation.Z-std::sin(0.25)) < 1e-6 && a.Rotation.Z == b.Rotation.Z,
          "free angular motion is integrated deterministically");
    const std::array touching{Box({0,-0.1f,0},{2,0.1f,2},0),Box({0,0.5f,0},{0.5f,0.5f,0.5f},1)};
    const auto query = lv::StepRigidBoxes(touching,{0});
    Check(query.Contacts.size()==4 && query.Contacts[0].Normal.Y > 0.999f && query.Boxes[1].Position.Y == 0.5f,
          "zero duration queries the oriented contact manifold without stepping");
    std::array bars{Box({},{1,0.12f,0.08f},0),Box({0.25f,0,0.25f},{1,0.12f,0.08f},1)};
    for (auto& bar : bars) { bar.Rotation = {0,0.38268343f,0,0.92387953f}; }
    Check(lv::StepRigidBoxes(bars,{0}).Contacts.empty(),"overlapping axis-aligned bounds do not imply oriented box contact");
    bars[1].Position = {0.09f,0,0.09f};
    const auto diagonal = lv::StepRigidBoxes(bars,{0}).Contacts;
    Check(diagonal.size()==4 && diagonal[0].Normal.X>0.70f && diagonal[0].Normal.Z>0.70f,
          "oriented contact normal follows the rotated face");
    auto invalid = boxes;
    invalid[0].Friction = -1;
    try { (void)lv::StepRigidBoxes(invalid); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("negative friction must fail validation");
}
} // namespace

int main() {
    try {
        Overhang();
        DynamicContact();
        RotationAndValidation();
        std::puts("Rigid box contact, friction, free rotation and pallet overhang passed.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}
