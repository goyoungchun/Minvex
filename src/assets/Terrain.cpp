#include <minvex/Terrain.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>

namespace lv {
namespace {
constexpr double MaxLocalMetres = 10'000'000;
void CheckHeight(double value, double origin) {
    if (!std::isfinite(value) || std::abs(value) > MaxLocalMetres || !std::isfinite(origin+value)) {
        throw std::invalid_argument("Local terrain height must be finite and within +/-10000000 metres.");
    }
}
bool Finite(SurveyPoint point) {
    return std::isfinite(point.Easting) && std::isfinite(point.Northing) && std::isfinite(point.Elevation);
}
double Noise(std::uint32_t x, std::uint32_t z, std::uint32_t seed) {
    auto hash = seed ^ (x*0x9e3779b9u) ^ (z*0x85ebca6bu);
    hash = (hash^(hash>>16))*0x7feb352du;
    hash = (hash^(hash>>15))*0x846ca68bu;
    return (hash^(hash>>16))/4294967295.0;
}
double SmoothNoise(double x, double z, std::uint32_t seed) {
    const auto ix = static_cast<std::uint32_t>(x), iz = static_cast<std::uint32_t>(z);
    const auto smooth = [](double t) { return t*t*(3-2*t); };
    const double u = smooth(x-ix), v = smooth(z-iz);
    return std::lerp(std::lerp(Noise(ix,iz,seed),Noise(ix+1,iz,seed),u),
                     std::lerp(Noise(ix,iz+1,seed),Noise(ix+1,iz+1,seed),u),v);
}
} // namespace

SurveyDistance MeasureSurvey(SurveyPoint from, SurveyPoint to) {
    const double horizontal = std::hypot(to.Easting-from.Easting,to.Northing-from.Northing);
    const double elevation = to.Elevation-from.Elevation;
    const double straight = std::hypot(horizontal,elevation);
    if (!std::isfinite(straight)) { throw std::invalid_argument("Survey distance exceeds the finite range."); }
    return {horizontal,elevation,straight};
}

TerrainGrid::TerrainGrid(TerrainLayout layout, std::span<const double> heights) : layout_(layout) {
    if (layout.Columns < 2 || layout.Rows < 2 || layout.Columns > MaxTerrainSamples/layout.Rows ||
        !std::isfinite(layout.SpacingX) || !std::isfinite(layout.SpacingZ) ||
        layout.SpacingX < 0.00001 || layout.SpacingZ < 0.00001 || Width() > MaxLocalMetres || Depth() > MaxLocalMetres ||
        !Finite(layout.Origin) || !std::isfinite(layout.Origin.Easting+Width()) || !std::isfinite(layout.Origin.Northing+Depth())) {
        throw std::invalid_argument("Terrain requires 2D dimensions, finite origin, supported spacing and at most 262144 samples.");
    }
    const auto count = layout.Columns*layout.Rows;
    if (!heights.empty() && heights.size() != count) { throw std::invalid_argument("Terrain height count must match the layout."); }
    for (const auto height : heights) { CheckHeight(height,layout.Origin.Elevation); }
    heights_.resize(count);
    std::copy(heights.begin(),heights.end(),heights_.begin());
}

void TerrainGrid::SetHeight(std::size_t column, std::size_t row, double height) {
    if (column >= layout_.Columns || row >= layout_.Rows) { throw std::out_of_range("Terrain sample is outside the grid."); }
    CheckHeight(height,layout_.Origin.Elevation);
    heights_[row*layout_.Columns+column] = height;
}

void TerrainGrid::GenerateMountain(const MountainPreset& preset) {
    CheckHeight(preset.MaximumHeight,layout_.Origin.Elevation);
    if (preset.MaximumHeight < 0 || !std::isfinite(preset.Ruggedness) || preset.Ruggedness < 0 || preset.Ruggedness > 1) {
        throw std::invalid_argument("Mountain height must be nonnegative and ruggedness must be in [0,1].");
    }
    if (layout_.Columns < 3 || layout_.Rows < 3) {
        throw std::invalid_argument("Mountains require at least three samples on each axis for an interior peak.");
    }
    double peak = 0;
    for (std::size_t row=0; row<layout_.Rows; ++row) {
        for (std::size_t col=0; col<layout_.Columns; ++col) {
            const double x = double(col)/(layout_.Columns-1), z = double(row)/(layout_.Rows-1);
            const double envelope = std::clamp(1-std::hypot(2*x-1,2*z-1),0.0,1.0);
            const double ridge = 1-std::abs(2*SmoothNoise(x*4,z*4,preset.Seed)-1);
            const double detail = SmoothNoise(x*12,z*12,preset.Seed+1u);
            const double shape = envelope*std::lerp(1.0,0.25+0.55*ridge+0.2*detail,preset.Ruggedness);
            heights_[row*layout_.Columns+col] = shape;
            peak = std::max(peak,shape);
        }
    }
    for (auto& height : heights_) { height = height/peak*preset.MaximumHeight; }
}

bool TerrainGrid::ApplyBrush(const TerrainBrush& brush) {
    const bool smooth = brush.Mode == TerrainBrushMode::Smooth;
    if (!std::isfinite(brush.X) || !std::isfinite(brush.Z) || !std::isfinite(brush.Radius) || brush.Radius <= 0 ||
        !std::isfinite(brush.Strength) || brush.Strength < 0 || (smooth && brush.Strength > 1) ||
        (!smooth && brush.Mode != TerrainBrushMode::Raise && brush.Mode != TerrainBrushMode::Lower)) {
        throw std::invalid_argument("Invalid terrain brush center, radius, strength or mode.");
    }
    const auto columns = layout_.Columns, rows = layout_.Rows;
    auto edited = heights_;
    bool changed = false;
    for (std::size_t row=0; row<rows; ++row) {
        for (std::size_t col=0; col<columns; ++col) {
            const auto index = row*columns+col;
            const double distance = std::hypot(col*layout_.SpacingX-brush.X,row*layout_.SpacingZ-brush.Z);
            if (distance >= brush.Radius) { continue; }
            const double weight = brush.Strength*(1-distance/brush.Radius);
            double height = heights_[index];
            if (smooth) {
                const double average = (heights_[row*columns+(col ? col-1 : col)] +
                    heights_[row*columns+std::min(col+1,columns-1)] + heights_[(row ? row-1 : row)*columns+col] +
                    heights_[std::min(row+1,rows-1)*columns+col])/4;
                height = std::lerp(height,average,weight);
            } else { height += brush.Mode == TerrainBrushMode::Raise ? weight : -weight; }
            CheckHeight(height,layout_.Origin.Elevation);
            edited[index] = height;
            changed |= height != heights_[index];
        }
    }
    if (changed) { heights_.swap(edited); }
    return changed;
}

std::array<std::size_t,3> TerrainGrid::Triangle(std::size_t triangle) const noexcept {
    const auto cell = triangle/2, columns = layout_.Columns;
    const auto a = cell/(columns-1)*columns+cell%(columns-1);
    return triangle%2 == 0 ? std::array{a,a+columns,a+1} : std::array{a+1,a+columns,a+columns+1};
}

SurveyPoint TerrainGrid::Survey(std::size_t triangle, const std::array<double,3>& barycentric) const {
    const auto indices = Triangle(triangle);
    SurveyPoint local;
    for (std::size_t i=0; i<indices.size(); ++i) {
        const auto index = indices[i];
        local.Easting += (index%layout_.Columns)*layout_.SpacingX*barycentric[i];
        local.Northing += (index/layout_.Columns)*layout_.SpacingZ*barycentric[i];
        local.Elevation += heights_[index]*barycentric[i];
    }
    return {layout_.Origin.Easting+local.Easting,layout_.Origin.Northing+local.Northing,layout_.Origin.Elevation+local.Elevation};
}

std::optional<SurveyPoint> TerrainGrid::Sample(double x, double z) const {
    if (!std::isfinite(x) || !std::isfinite(z)) { throw std::invalid_argument("Terrain sample coordinates must be finite."); }
    if (x < 0 || z < 0 || x > Width() || z > Depth()) { return std::nullopt; }
    const double column = std::min(x/layout_.SpacingX,double(layout_.Columns-1));
    const double row = std::min(z/layout_.SpacingZ,double(layout_.Rows-1));
    const auto colCell = std::min(static_cast<std::size_t>(column),layout_.Columns-2);
    const auto rowCell = std::min(static_cast<std::size_t>(row),layout_.Rows-2);
    const auto first = 2*(rowCell*(layout_.Columns-1)+colCell);
    const double u = column-colCell, v = row-rowCell;
    if (u+v <= 1) { return Survey(first,{1-u-v,v,u}); }
    return Survey(first+1,{1-v,1-u,u+v-1});
}

SurveyPoint TerrainGrid::SampleTriangle(std::size_t triangle, const std::array<double,3>& barycentric) const {
    constexpr double tolerance = 1e-10;
    if (triangle >= 2*(layout_.Columns-1)*(layout_.Rows-1) ||
        std::abs(std::accumulate(barycentric.begin(),barycentric.end(),0.0)-1) > tolerance ||
        std::any_of(barycentric.begin(),barycentric.end(),[](double value) {
            return !std::isfinite(value) || value < -tolerance || value > 1+tolerance;
        })) { throw std::invalid_argument("Invalid terrain triangle or barycentric coordinates."); }
    return Survey(triangle,barycentric);
}

MeshData TerrainGrid::MakeMesh() const {
    const auto columns = layout_.Columns, rows = layout_.Rows;
    MeshData mesh;
    mesh.Vertices.reserve(heights_.size());
    const auto [minHeight,maxHeight] = std::minmax_element(heights_.begin(),heights_.end());
    for (std::size_t row=0; row<rows; ++row) {
        for (std::size_t col=0; col<columns; ++col) {
            const auto west = col ? col-1 : col, east = std::min(col+1,columns-1);
            const auto south = row ? row-1 : row, north = std::min(row+1,rows-1);
            const double dx = (heights_[row*columns+east]-heights_[row*columns+west])/((east-west)*layout_.SpacingX);
            const double dz = (heights_[north*columns+col]-heights_[south*columns+col])/((north-south)*layout_.SpacingZ);
            const double length = std::hypot(dx,1.0,dz);
            const double height = heights_[row*columns+col];
            const float shade = *minHeight < *maxHeight ? static_cast<float>((height-*minHeight)/(*maxHeight-*minHeight)) : 0;
            mesh.Vertices.push_back({{static_cast<float>(col*layout_.SpacingX),static_cast<float>(height),static_cast<float>(row*layout_.SpacingZ)},
                {static_cast<float>(-dx/length),static_cast<float>(1/length),static_cast<float>(-dz/length)},
                {0.24f+0.58f*shade,0.48f+0.31f*shade,0.29f+0.44f*shade,1},
                {static_cast<float>(double(col)/(columns-1)),static_cast<float>(double(row)/(rows-1))}});
        }
    }
    const auto triangles = 2*(columns-1)*(rows-1);
    mesh.Indices.reserve(3*triangles);
    for (std::size_t triangle=0; triangle<triangles; ++triangle) {
        for (const auto index : Triangle(triangle)) { mesh.Indices.push_back(static_cast<std::uint32_t>(index)); }
    }
    return mesh;
}

std::vector<TerrainContour> TerrainGrid::Contours(double interval, std::size_t maxSegments) const {
    if (!std::isfinite(interval) || interval < 0.00001 || maxSegments > 65536) {
        throw std::invalid_argument("Contours require an interval of at least 0.00001 metres and at most 65536 segments.");
    }
    std::vector<TerrainContour> contours;
    std::unordered_set<std::uint64_t> levelEdges;
    const auto triangles = 2*(layout_.Columns-1)*(layout_.Rows-1);
    for (std::size_t triangle=0; triangle<triangles; ++triangle) {
        const auto indices = Triangle(triangle);
        const std::array heights{heights_[indices[0]],heights_[indices[1]],heights_[indices[2]]};
        const auto [low,high] = std::minmax_element(heights.begin(),heights.end());
        const auto first = static_cast<std::int64_t>(std::floor(*low/interval))+1;
        const auto last = static_cast<std::int64_t>(std::floor(*high/interval));
        if (last-first > static_cast<std::int64_t>(maxSegments)+1) { throw std::length_error("Contour segment limit exceeded."); }
        for (auto step=first; step<=last; ++step) {
            const double level = step*interval;
            std::array<Vec3,2> points{};
            std::array<std::size_t,2> vertices{};
            std::size_t count = 0;
            for (std::size_t i=0; i<3; ++i) {
                const auto j = (i+1)%3;
                if ((heights[i] < level) == (heights[j] < level)) { continue; }
                const auto a = indices[i], b = indices[j];
                const double t = (level-heights[i])/(heights[j]-heights[i]);
                points[count] = {static_cast<float>(std::lerp(double(a%layout_.Columns),double(b%layout_.Columns),t)*layout_.SpacingX),
                                 static_cast<float>(level),
                                 static_cast<float>(std::lerp(double(a/layout_.Columns),double(b/layout_.Columns),t)*layout_.SpacingZ)};
                vertices[count++] = t == 0 ? a : t == 1 ? b : heights_.size();
            }
            if (count != 2 || (points[0].X == points[1].X && points[0].Z == points[1].Z)) { continue; }
            if (vertices[0] != heights_.size() && vertices[1] != heights_.size()) {
                const auto [a,b] = std::minmax(vertices[0],vertices[1]);
                if (!levelEdges.insert((std::uint64_t(a)<<32)|b).second) { continue; }
            }
            if (contours.size() >= maxSegments) { throw std::length_error("Contour segment limit exceeded."); }
            contours.push_back({points[0],points[1]});
        }
    }
    return contours;
}

ModelHandle MakeTerrainModel(const TerrainGrid& grid) {
    auto model = std::make_shared<ModelAsset>();
    model->Format = AssetFormat::Custom;
    model->Terrain = std::make_shared<TerrainGrid>(grid);
    model->Meshes.push_back({"Terrain",{{grid.MakeMesh(),0,false}}});
    model->Parts.push_back({0});
    MaterialAsset material;
    material.Name = "Terrain";
    material.Shading = ShadingModel::Pbr;
    material.MetallicFactor = 0;
    material.RoughnessFactor = 0.9f;
    model->Materials.push_back(std::move(material));
    const auto heights = grid.Heights();
    const auto [low,high] = std::minmax_element(heights.begin(),heights.end());
    model->ModelBounds = {{0,static_cast<float>(*low),0},
        {static_cast<float>(grid.Width()),static_cast<float>(*high),static_cast<float>(grid.Depth())}};
    ValidateModel(*model);
    return model;
}
} // namespace lv
