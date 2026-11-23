#include <minvex/Terrain.h>

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <sstream>

namespace lv {
namespace {
constexpr std::uintmax_t MaxTerrainFileBytes = 32*1024*1024;
std::size_t FileSize(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size == 0 || size > MaxTerrainFileBytes) { throw std::invalid_argument("Terrain files must contain 1 byte to 32 MiB."); }
    return static_cast<std::size_t>(size);
}
std::string_view Trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) { return {}; }
    return text.substr(first,text.find_last_not_of(" \t\r")-first+1);
}
SurveyPoint ParseRow(std::string_view row) {
    const auto first = row.find(',');
    const auto second = first == std::string_view::npos ? first : row.find(',',first+1);
    if (second == std::string_view::npos || row.find(',',second+1) != std::string_view::npos) {
        throw std::invalid_argument("Terrain CSV rows require easting,northing,elevation.");
    }
    const auto number = [](std::string_view field) {
        std::istringstream input{std::string(Trim(field))};
        input.imbue(std::locale::classic());
        double value;
        if (!(input >> value) || !input.eof() || !std::isfinite(value)) {
            throw std::invalid_argument("Terrain CSV fields must be finite decimal numbers.");
        }
        return value;
    };
    return {number(row.substr(0,first)),number(row.substr(first+1,second-first-1)),number(row.substr(second+1))};
}
double RegularSpacing(std::vector<double>& axis) {
    std::sort(axis.begin(),axis.end());
    axis.erase(std::unique(axis.begin(),axis.end()),axis.end());
    if (axis.size() < 2) { throw std::invalid_argument("Terrain CSV needs at least two samples on each axis."); }
    const double spacing = (axis.back()-axis.front())/(axis.size()-1);
    const double coordinateScale = std::max(std::abs(axis.front()),std::abs(axis.back()));
    const double tolerance = std::max(spacing*1e-7,coordinateScale*std::numeric_limits<double>::epsilon()*4);
    if (!std::isfinite(spacing) || spacing <= tolerance) { throw std::invalid_argument("Terrain CSV spacing cannot be resolved at this origin."); }
    for (std::size_t i=1; i<axis.size(); ++i) {
        if (std::abs((axis[i]-axis.front())-i*spacing) > tolerance) {
            throw std::invalid_argument("Terrain CSV coordinates must form a regular grid.");
        }
    }
    return spacing;
}
} // namespace

TerrainGrid LoadTerrainCsv(const std::filesystem::path& path) {
    const auto size = FileSize(path);
    std::string bytes(size,'\0');
    std::ifstream input(path,std::ios::binary);
    if (!input.read(bytes.data(),static_cast<std::streamsize>(size))) { throw std::runtime_error("Could not read terrain CSV."); }
    std::istringstream csv(std::move(bytes));
    std::string line;
    if (!std::getline(csv,line)) { throw std::invalid_argument("Expected a terrain CSV header."); }
    if (line.starts_with("\xef\xbb\xbf")) { line.erase(0,3); }
    if (Trim(line) != "easting,northing,elevation") { throw std::invalid_argument("Expected the header easting,northing,elevation."); }
    std::vector<SurveyPoint> points;
    std::vector<double> eastings,northings;
    double minHeight = std::numeric_limits<double>::infinity();
    while (std::getline(csv,line)) {
        const auto row = Trim(line);
        if (row.empty()) { continue; }
        if (points.size() == MaxTerrainSamples || row.size() > 256) { throw std::length_error("Terrain CSV row or sample limit exceeded."); }
        const auto point = ParseRow(row);
        points.push_back(point);
        eastings.push_back(point.Easting);
        northings.push_back(point.Northing);
        minHeight = std::min(minHeight,point.Elevation);
    }
    const auto dx = RegularSpacing(eastings), dz = RegularSpacing(northings);
    if (eastings.size() > points.size()/northings.size() || eastings.size()*northings.size() != points.size()) {
        throw std::invalid_argument("Terrain CSV must contain one row for each grid point.");
    }
    const auto columns = eastings.size();
    std::vector<double> heights(points.size(),std::numeric_limits<double>::quiet_NaN());
    for (const auto& point : points) {
        const auto col = std::lower_bound(eastings.begin(),eastings.end(),point.Easting)-eastings.begin();
        const auto row = std::lower_bound(northings.begin(),northings.end(),point.Northing)-northings.begin();
        auto& height = heights[row*columns+col];
        if (!std::isnan(height)) { throw std::invalid_argument("Terrain CSV contains a duplicate grid point."); }
        height = point.Elevation-minHeight;
    }
    return TerrainGrid({columns,northings.size(),dx,dz,{eastings.front(),northings.front(),minHeight}},heights);
}

TerrainGrid LoadTerrainHeightmap(const std::filesystem::path& path, double spacingX, double spacingZ,
                                 double heightRange, SurveyPoint origin) {
    if (!std::isfinite(heightRange) || heightRange < 0 || heightRange > 10'000'000) {
        throw std::invalid_argument("Heightmap range must be in [0,10000000] metres.");
    }
    std::vector<stbi_uc> bytes(FileSize(path));
    std::ifstream input(path,std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("Could not read terrain heightmap.");
    }
    const bool png = bytes.size() >= 8 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G';
    int width = 0, height = 0, channels = 0;
    const auto size = static_cast<int>(bytes.size());
    if (!png || !stbi_info_from_memory(bytes.data(),size,&width,&height,&channels) ||
        channels < 1 || channels > 2 || width < 2 || height < 2 ||
        static_cast<std::size_t>(width) > MaxTerrainSamples/static_cast<std::size_t>(height)) {
        throw std::invalid_argument("Expected a grayscale PNG within the terrain grid limit.");
    }
    std::unique_ptr<stbi_us,decltype(&stbi_image_free)> samples(
        stbi_load_16_from_memory(bytes.data(),size,&width,&height,&channels,1),stbi_image_free);
    if (!samples) { throw std::invalid_argument("Could not decode the terrain heightmap PNG."); }
    std::vector<double> heights(static_cast<std::size_t>(width)*height);
    for (std::size_t i=0; i<heights.size(); ++i) { heights[i] = heightRange*samples.get()[i]/65535.0; }
    return TerrainGrid({static_cast<std::size_t>(width),static_cast<std::size_t>(height),spacingX,spacingZ,origin},heights);
}
} // namespace lv
