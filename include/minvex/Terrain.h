#pragma once

#include <minvex/Asset.h>

namespace lv {

inline constexpr std::size_t MaxTerrainSamples = 262144;
struct SurveyPoint { double Easting = 0, Northing = 0, Elevation = 0; };
struct TerrainLayout {
    std::size_t Columns = 2, Rows = 2;
    double SpacingX = 1, SpacingZ = 1; // Metres between samples.
    SurveyPoint Origin{};
};
struct MountainPreset { double MaximumHeight = 20, Ruggedness = 0.65; std::uint32_t Seed = 1; };
enum class TerrainBrushMode { Raise, Lower, Smooth };
struct TerrainBrush {
    TerrainBrushMode Mode = TerrainBrushMode::Raise;
    double X = 0, Z = 0, Radius = 2, Strength = 1; // Strength in metres; [0,1] for Smooth.
};
struct TerrainContour { Vec3 Start, End; };
struct SurveyDistance { double Horizontal = 0, ElevationChange = 0, Straight = 0; };
SurveyDistance MeasureSurvey(SurveyPoint from, SurveyPoint to);

// Owned, single-caller heightfield. Local X/Z map to easting/northing and
// local Y to elevation above Origin. Samples are local heights, row-major.
class TerrainGrid {
public:
    explicit TerrainGrid(TerrainLayout layout = {}, std::span<const double> heights = {});
    const TerrainLayout& Layout() const noexcept { return layout_; }
    std::span<const double> Heights() const noexcept { return heights_; }
    double Width() const noexcept { return (layout_.Columns-1)*layout_.SpacingX; }
    double Depth() const noexcept { return (layout_.Rows-1)*layout_.SpacingZ; }
    void SetHeight(std::size_t column, std::size_t row, double height);
    void GenerateMountain(const MountainPreset& preset); // Requires at least 3 x 3 samples.
    bool ApplyBrush(const TerrainBrush& brush);
    std::optional<SurveyPoint> Sample(double x, double z) const;
    SurveyPoint SampleTriangle(std::size_t triangle, const std::array<double,3>& barycentric) const;
    MeshData MakeMesh() const;
    std::vector<TerrainContour> Contours(double interval, std::size_t maxSegments = 65536) const;

private:
    std::array<std::size_t,3> Triangle(std::size_t triangle) const noexcept;
    SurveyPoint Survey(std::size_t triangle, const std::array<double,3>& barycentric) const;
    TerrainLayout layout_;
    std::vector<double> heights_;
};

// CSV columns: easting,northing,elevation. Complete, regular grid; row order is free.
TerrainGrid LoadTerrainCsv(const std::filesystem::path& path);
// Grayscale PNG heightmap (8 or 16 bits). First image row maps to local Z=0.
TerrainGrid LoadTerrainHeightmap(const std::filesystem::path& path, double spacingX, double spacingZ,
                                 double heightRange, SurveyPoint origin = {});
// Deep-copy an immutable mesh/heightfield snapshot suitable for Scene::AddModel/SetModel.
ModelHandle MakeTerrainModel(const TerrainGrid& grid);

} // namespace lv
