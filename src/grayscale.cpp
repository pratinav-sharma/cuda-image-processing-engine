#include "grayscale.h"
#include <stdexcept>

namespace image {
Bytes grayscale_cpu(const Bytes& rgb) {
    if (rgb.size() % 3 != 0) throw std::invalid_argument("RGB input must contain complete pixels");
    Bytes gray(rgb.size() / 3);
    for (std::size_t pixel = 0; pixel < gray.size(); ++pixel) {
        const unsigned red = rgb[3 * pixel];
        const unsigned green = rgb[3 * pixel + 1];
        const unsigned blue = rgb[3 * pixel + 2];
        gray[pixel] = static_cast<std::uint8_t>((77 * red + 150 * green + 29 * blue + 128) / 256);
    }
    return gray;
}
}
