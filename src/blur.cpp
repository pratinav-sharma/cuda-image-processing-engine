#include "blur.h"
#include <algorithm>
#include <stdexcept>

namespace image {
void validate_blur_input(const Bytes& gray, int width, int height) {
    if (width < 1 || height < 1 || width > 16384 || height > 16384)
        throw std::invalid_argument("Blur dimensions must be between 1 and 16384");
    const auto pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixels > 64u * 1024u * 1024u || gray.size() != pixels)
        throw std::invalid_argument("Blur requires matching grayscale pixels, at most 64 Mi pixels");
}
Bytes box_blur_cpu(const Bytes& gray, int width, int height) {
    validate_blur_input(gray, width, height);
    Bytes output(gray.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned sum = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                const int row = std::clamp(y + dy, 0, height - 1);
                for (int dx = -1; dx <= 1; ++dx) {
                    const int column = std::clamp(x + dx, 0, width - 1);
                    sum += gray[static_cast<std::size_t>(row) * width + column];
                }
            }
            output[static_cast<std::size_t>(y) * width + x] = static_cast<std::uint8_t>((sum + 4u) / 9u);
        }
    }
    return output;
}
}
