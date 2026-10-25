#include "grayscale.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        image::print_device_info();
        constexpr std::size_t width = 1920, height = 1080;
        image::Bytes rgb(width * height * 3);
        for (std::size_t y = 0; y < height; ++y)
            for (std::size_t x = 0; x < width; ++x) {
                const auto offset = 3 * (y * width + x);
                rgb[offset] = static_cast<std::uint8_t>(x % 256);
                rgb[offset + 1] = static_cast<std::uint8_t>(y % 256);
                rgb[offset + 2] = static_cast<std::uint8_t>((x + y) % 256);
            }
        const auto gpu = image::grayscale_gpu(rgb);
        if (gpu != image::grayscale_cpu(rgb)) throw std::runtime_error("CPU/GPU mismatch");
        std::cout << "PASS: executed RGB-to-grayscale CUDA kernel on " << width << 'x' << height
                  << " image; all " << gpu.size() << " pixels match the CPU reference.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
