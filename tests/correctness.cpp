#include "grayscale.h"
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main() {
    try {
        image::print_device_info();
        // Independent known values keep a shared CPU/GPU formula error from passing.
        const image::Bytes colors{0,0,0, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 128,128,128};
        const image::Bytes expected{0,255,77,149,29,128};
        require(image::grayscale_cpu(colors) == expected, "CPU known-color mismatch");
        require(image::grayscale_gpu(colors) == expected, "GPU known-color mismatch");
        require(image::grayscale_cpu({}).empty(), "CPU empty-input mismatch");
        require(image::grayscale_gpu({}).empty(), "GPU empty-input mismatch");
        for (const auto convert : {image::grayscale_cpu, image::grayscale_gpu}) {
            bool rejected = false;
            try { convert({1,2}); } catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Incomplete RGB pixel was accepted");
        }
        std::mt19937 random(20261025);
        const std::pair<std::size_t, std::size_t> sizes[] = {
            {1,1}, {17,13}, {255,1}, {256,1}, {257,1}, {641,479}, {1920,1080}
        };
        std::size_t cases = 0;
        for (const auto& size : sizes) {
            for (int pattern = 0; pattern < 4; ++pattern) {
                image::Bytes rgb(size.first * size.second * 3);
                for (std::size_t i = 0; i < rgb.size(); ++i) {
                    rgb[i] = pattern == 0 ? 0 : pattern == 1 ? 255 : pattern == 2
                        ? static_cast<std::uint8_t>(((i / 3) % 2) * 255)
                        : static_cast<std::uint8_t>(random() & 255u);
                }
                require(image::grayscale_gpu(rgb) == image::grayscale_cpu(rgb), "GPU/CPU pixel mismatch");
                ++cases;
            }
        }
        std::cout << "PASS: known colors, empty/invalid inputs, and " << cases
                  << " GPU/CPU image comparisons (including partial CUDA blocks).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
