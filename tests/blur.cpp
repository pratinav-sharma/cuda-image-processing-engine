#include "blur.h"
#include <iostream>
#include <stdexcept>
#include <array>
#include <random>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void rejects(Function function) {
    try { function(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected invalid blur input to be rejected");
}
void check_known(const image::Bytes& input, int width, int height, const image::Bytes& expected) {
    expect(image::box_blur_cpu(input, width, height) == expected, "CPU blur differs from hand-calculated result");
    expect(image::box_blur_gpu(input, width, height) == expected, "GPU blur differs from hand-calculated result");
    expect(image::box_blur_gpu(input, width, height, image::BlurKernel::Shared) == expected,
           "Shared GPU blur differs from hand-calculated result");
}
}
int main() {
    try {
        image::print_device_info();
        check_known({123}, 1, 1, {123});
        check_known({0, 90, 180}, 3, 1, {30, 90, 150});
        check_known({0, 90, 180}, 1, 3, {30, 90, 150});
        check_known({0, 90, 180, 255}, 2, 2, {88, 117, 147, 173});
        check_known({0, 0, 0, 0, 255, 0, 0, 0, 0}, 3, 3, image::Bytes(9, 28));
        // Values around the nearest-integer boundary: four rounds to zero, five to one.
        check_known({0, 0, 0, 0, 4, 0, 0, 0, 0}, 3, 3, image::Bytes(9, 0));
        check_known({0, 0, 0, 0, 5, 0, 0, 0, 0}, 3, 3, image::Bytes(9, 1));
        check_known(image::Bytes(17 * 19, 255), 17, 19, image::Bytes(17 * 19, 255));
        rejects([] { image::box_blur_cpu({}, 0, 0); });
        rejects([] { image::box_blur_cpu({0}, -1, 1); });
        rejects([] { image::box_blur_cpu({0}, 2, 2); });
        rejects([] { image::box_blur_cpu({0}, 16385, 1); });
        rejects([] { image::box_blur_cpu({}, 16384, 16384); });
        rejects([] { image::box_blur_gpu({0}, 2, 2); });
        rejects([] { image::box_blur_gpu({0}, 1, 1, static_cast<image::BlurKernel>(99)); });
        const std::array<std::array<int, 2>, 12> shapes{{
            {1, 1}, {1, 19}, {19, 1}, {2, 2}, {3, 3}, {15, 15},
            {16, 16}, {17, 17}, {31, 33}, {33, 31}, {65, 19}, {127, 73}
        }};
        std::mt19937 random(2026);
        for (const auto& shape : shapes) {
            const int width = shape[0], height = shape[1];
            for (int pattern = 0; pattern < 5; ++pattern) {
                image::Bytes input(static_cast<std::size_t>(width) * height);
                for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                    const auto index = static_cast<std::size_t>(y) * width + x;
                    input[index] = pattern == 0 ? 0 : pattern == 1 ? 255 :
                        pattern == 2 ? ((x + y) % 2 ? 255 : 0) :
                        pattern == 3 ? ((x == width / 2 && y == height / 2) ? 255 : 0) :
                        static_cast<std::uint8_t>(random() & 255u);
                }
                const auto before = input;
                expect(image::box_blur_gpu(input, width, height) == image::box_blur_cpu(input, width, height),
                       "GPU/CPU blur mismatch on generated input");
                expect(image::box_blur_gpu(input, width, height, image::BlurKernel::Shared) == image::box_blur_cpu(input, width, height),
                       "Shared GPU/CPU blur mismatch on generated input");
                expect(input == before, "Blur modified its input");
            }
        }
        std::cout << "PASS: known blur values, edge clamping, rounding, invalid input, and 120 generated GPU/CPU comparisons.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
