#include "image_io.h"
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {
constexpr std::size_t max_pixels = 64u * 1024u * 1024u;
constexpr std::size_t max_file_bytes = 256u * 1024u * 1024u;

std::size_t pixel_count(int width, int height) {
    if (width <= 0 || height <= 0 || width > STBI_MAX_DIMENSIONS || height > STBI_MAX_DIMENSIONS)
        throw std::invalid_argument("Image dimensions must be between 1 and 16384");
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count > max_pixels) throw std::invalid_argument("Image exceeds the 64-megapixel limit");
    return count;
}
}

namespace image {
RgbImage load_rgb(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Cannot open input image: " + path.u8string());
    const auto size = input.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(max_file_bytes))
        throw std::runtime_error("Input must be a nonempty image file of at most 256 MiB");
    Bytes encoded(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size())))
        throw std::runtime_error("Failed to read input image: " + path.u8string());

    int width = 0, height = 0, channels = 0;
    const auto length = static_cast<int>(encoded.size());
    if (!stbi_info_from_memory(encoded.data(), length, &width, &height, &channels))
        throw std::runtime_error("Cannot decode image header (expected PNG, JPEG, or BMP)");
    const auto count = pixel_count(width, height);
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(encoded.data(), length, &width, &height, &channels, STBI_rgb), stbi_image_free);
    if (!decoded) throw std::runtime_error(std::string("Cannot decode image: ") + stbi_failure_reason());
    if (pixel_count(width, height) != count) throw std::runtime_error("Image dimensions changed during decoding");
    return {width, height, Bytes(decoded.get(), decoded.get() + count * 3)};
}

void save_grayscale_png(const std::filesystem::path& path, int width, int height, const Bytes& gray) {
    if (gray.size() != pixel_count(width, height))
        throw std::invalid_argument("Grayscale buffer size does not match image dimensions");
    auto extension = path.extension().u8string();
    for (auto& c : extension) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    if (extension != ".png") throw std::invalid_argument("Output filename must end in .png");
    if (std::filesystem::exists(path))
        throw std::runtime_error("Output already exists; choose a new filename: " + path.u8string());

    int encoded_size = 0;
    std::unique_ptr<unsigned char, decltype(&std::free)> encoded(
        stbi_write_png_to_mem(gray.data(), width, width, height, 1, &encoded_size), std::free);
    if (!encoded) throw std::runtime_error("Failed to encode grayscale PNG");
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot create output image: " + path.u8string());
    output.write(reinterpret_cast<const char*>(encoded.get()), encoded_size);
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw std::runtime_error("Failed to write output image: " + path.u8string());
    }
}
}
