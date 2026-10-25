#pragma once
#include "grayscale.h"
#include <filesystem>

namespace image {
struct RgbImage {
    int width;
    int height;
    Bytes pixels;
};

// Decode PNG/JPEG/BMP to packed, top-to-bottom RGB8. Alpha is discarded.
RgbImage load_rgb(const std::filesystem::path& path);
// Write a single-channel 8-bit PNG. Existing files are never intentionally replaced.
void save_grayscale_png(const std::filesystem::path& path, int width, int height, const Bytes& gray);
}
