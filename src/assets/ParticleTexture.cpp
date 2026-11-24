#include <minvex/Particles.h>

#include <stb_image.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace lv {

ParticleTextureHandle LoadParticleTexture(const std::filesystem::path& path) {
    constexpr std::uintmax_t maxBytes = 8 * 1024 * 1024;
    if (path.empty()) { throw std::invalid_argument("Choose a particle PNG file."); }
    const auto canonical = std::filesystem::canonical(path);
    const auto size = std::filesystem::file_size(canonical);
    if (size < 8 || size > maxBytes) { throw std::length_error("Particle PNG size must be between 8 bytes and 8 MiB."); }
    std::vector<stbi_uc> bytes(static_cast<std::size_t>(size));
    std::ifstream stream(canonical, std::ios::binary);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("Cannot read particle PNG.");
    }
    constexpr std::array<stbi_uc, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    if (!std::equal(signature.begin(), signature.end(), bytes.begin())) {
        throw std::invalid_argument("Particle textures require PNG data.");
    }
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(bytes.data(), static_cast<int>(size), &width, &height, &channels) ||
        width < 1 || height < 1 || width > MaxParticleTextureSize || height > MaxParticleTextureSize) {
        throw std::invalid_argument("Particle PNG dimensions must be in [1,2048].");
    }
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> data(
        stbi_load_from_memory(bytes.data(), static_cast<int>(size), &width, &height, &channels, 4), stbi_image_free);
    if (!data) { throw std::invalid_argument("Cannot decode particle PNG."); }
    auto texture = std::unique_ptr<ParticleTexture>(new ParticleTexture);
    texture->path_ = canonical;
    texture->width_ = width;
    texture->height_ = height;
    texture->pixels_.assign(data.get(), data.get() + static_cast<std::size_t>(width) * height * 4);
    return std::shared_ptr<ParticleTexture>(std::move(texture));
}

} // namespace lv
