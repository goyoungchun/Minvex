#include <minvex/Physics.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lv {
namespace {
struct Footprint { double West, East, South, North; };
Footprint Overlap(Bounds a, Bounds b) {
    return {std::max(a.Min.X,b.Min.X),std::min(a.Max.X,b.Max.X),
            std::max(a.Min.Z,b.Min.Z),std::min(a.Max.Z,b.Max.Z)};
}
bool Positive(Footprint f) { return f.West < f.East && f.South < f.North; }
void Validate(std::span<const PhysicsBox> boxes) {
    if (boxes.size() > MaxPhysicsBoxes) { throw std::length_error("Box physics supports up to 256 colliders."); }
    for (const auto& box : boxes) {
        const auto& [low,high] = box.Collider;
        const float coordinates[]{low.X,low.Y,low.Z,high.X,high.Y,high.Z};
        if (!(low.X < high.X && low.Y < high.Y && low.Z < high.Z) ||
            !std::ranges::all_of(coordinates,[](float value) { return std::isfinite(value) && std::abs(value) <= 10'000'000; })) {
            throw std::invalid_argument("Box collider requires finite increasing bounds within +/-10000000 metres.");
        }
    }
}

double UnionArea(const std::vector<Footprint>& contact) {
    std::vector<double> boundaries;
    for (const auto& f : contact) { boundaries.insert(boundaries.end(),{f.West,f.East}); }
    std::sort(boundaries.begin(),boundaries.end());
    const auto last = std::unique(boundaries.begin(),boundaries.end());
    double area = 0;
    std::vector<std::pair<double,double>> intervals;
    for (auto east = boundaries.begin(); east != last; ++east) {
        if (east == boundaries.begin()) { continue; }
        const double west = *std::prev(east);
        intervals.clear();
        for (const auto& f : contact) {
            if (f.West <= west && f.East >= *east) { intervals.emplace_back(f.South,f.North); }
        }
        std::sort(intervals.begin(),intervals.end());
        double north = -std::numeric_limits<double>::infinity();
        for (const auto& [low,high] : intervals) {
            area += (*east-west)*std::max(0.0,high-std::max(north,low));
            north = std::max(north,high);
        }
    }
    return area;
}
} // namespace

BoxSupport CheckBoxSupport(std::span<const PhysicsBox> boxes, std::size_t box, double tolerance) {
    Validate(boxes);
    if (box >= boxes.size() || !std::isfinite(tolerance) || tolerance < 0) {
        throw std::invalid_argument("Support requires a box index and finite nonnegative tolerance.");
    }
    const auto& bounds = boxes[box].Collider;
    const double centerX = (double(bounds.Min.X)+bounds.Max.X)/2;
    const double centerZ = (double(bounds.Min.Z)+bounds.Max.Z)/2;
    BoxSupport support;
    std::vector<Footprint> contact;
    for (std::size_t i=0; i<boxes.size(); ++i) {
        if (i == box) { continue; }
        const auto& other = boxes[i].Collider;
        const double gap = double(bounds.Min.Y)-other.Max.Y;
        const auto region = Overlap(bounds,other);
        if (gap < -tolerance || !Positive(region)) { continue; }
        support.Gap = std::min(support.Gap,std::max(0.0,gap));
        if (gap > tolerance) { continue; }
        contact.push_back(region);
        support.CenterOverContact |= centerX >= region.West && centerX <= region.East &&
                                     centerZ >= region.South && centerZ <= region.North;
    }
    const double area = (double(bounds.Max.X)-bounds.Min.X)*(double(bounds.Max.Z)-bounds.Min.Z);
    support.ContactFraction = std::clamp(UnionArea(contact)/area,0.0,1.0);
    return support;
}

std::vector<BoxGravityResult> PreviewBoxGravity(std::span<const PhysicsBox> boxes, GravityPreviewOptions options) {
    Validate(boxes);
    if (!std::isfinite(options.Seconds) || options.Seconds < 0 || options.Seconds > 60 ||
        !std::isfinite(options.Acceleration) || options.Acceleration < 0 || options.Acceleration > 100) {
        throw std::invalid_argument("Gravity preview requires 0..60 seconds and 0..100 m/s^2 acceleration.");
    }
    std::vector<BoxGravityResult> result;
    std::vector<std::size_t> moving, settled;
    for (std::size_t i=0; i<boxes.size(); ++i) {
        result.push_back({boxes[i],false});
        (boxes[i].Static ? settled : moving).push_back(i);
    }
    std::stable_sort(moving.begin(),moving.end(),[&](std::size_t a, std::size_t b) {
        return boxes[a].Collider.Min.Y < boxes[b].Collider.Min.Y;
    });
    const double fall = 0.5*options.Acceleration*options.Seconds*options.Seconds;
    for (const auto i : moving) {
        const auto& box = boxes[i].Collider;
        double floor = -std::numeric_limits<double>::infinity();
        for (const auto j : settled) {
            const auto& original = boxes[j].Collider;
            if (!Positive(Overlap(box,original))) { continue; }
            if (box.Min.Y < original.Max.Y && original.Min.Y < box.Max.Y) {
                throw std::invalid_argument("Gravity preview requires moving boxes with disjoint initial interiors.");
            }
            if (original.Max.Y <= box.Min.Y) { floor = std::max(floor,double(result[j].Box.Collider.Max.Y)); }
        }
        const double bottom = std::max(box.Min.Y-fall,floor);
        if (bottom < -10'000'000) { throw std::invalid_argument("Preview translation exceeds collider range."); }
        auto& next = result[i];
        next.InContact = bottom == floor;
        next.Box.Collider.Min.Y = static_cast<float>(bottom);
        next.Box.Collider.Max.Y = static_cast<float>(bottom+(double(box.Max.Y)-box.Min.Y));
        if (!(next.Box.Collider.Min.Y < next.Box.Collider.Max.Y)) {
            throw std::invalid_argument("Preview translation exceeds collider precision.");
        }
        settled.push_back(i);
    }
    return result;
}
} // namespace lv
