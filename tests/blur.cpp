#include "blur.h"
#include <iostream>
#include <stdexcept>

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
}
}
int main() {
    try {
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
        std::cout << "PASS: CPU box blur known values, edge clamping, rounding, and invalid dimensions.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
