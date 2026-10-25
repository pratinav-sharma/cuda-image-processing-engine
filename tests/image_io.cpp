#include "image_io.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Operation>
void expect_error(Operation operation, const char* message) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    require(rejected, message);
}
struct ScratchDirectory {
    std::filesystem::path path = std::filesystem::current_path() /
        ("image-io-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ScratchDirectory() {
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("Cannot create test directory");
    }
    ~ScratchDirectory() {
        // Only files created by this test; never recurse into another directory.
        std::error_code ignored;
        for (const auto& entry : std::filesystem::directory_iterator(path, ignored))
            if (entry.is_regular_file()) std::filesystem::remove(entry.path(), ignored);
        std::filesystem::remove(path, ignored);
    }
};
}

int main(int argc, char* argv[]) {
    try {
        require(argc == 2, "Expected fixture directory argument");
        const std::filesystem::path fixtures(argv[1]);
        const image::Bytes colors{255,0,0, 0,255,0, 0,0,255, 255,255,255, 0,0,0, 128,128,128};
        for (const auto* name : {"rgb.png", "rgb.bmp"}) {
            const auto loaded = image::load_rgb(fixtures / name);
            require(loaded.width == 3 && loaded.height == 2, "Decoded dimensions mismatch");
            require(loaded.pixels == colors, "RGB channels, row order, or padding mismatch");
        }
        const auto jpeg = image::load_rgb(fixtures / "rgb.jpg");
        require(jpeg.width == 3 && jpeg.height == 2 && jpeg.pixels.size() == 18, "JPEG decode mismatch");
        const auto rgba = image::load_rgb(fixtures / "rgba.png");
        require(rgba.width == 2 && rgba.height == 1 && rgba.pixels == image::Bytes{255,0,0, 0,0,255},
                "Alpha discard must preserve uncomposited RGB");

        ScratchDirectory scratch;
        const auto output = scratch.path / std::filesystem::u8path("gray result \xcf\x80.PNG");
        const image::Bytes gray{77,149,29,255,0,128};
        image::save_grayscale_png(output, 3, 2, gray);
        const auto roundtrip = image::load_rgb(output);
        require(roundtrip.width == 3 && roundtrip.height == 2, "PNG roundtrip dimensions mismatch");
        for (std::size_t pixel = 0; pixel < gray.size(); ++pixel)
            for (std::size_t channel = 0; channel < 3; ++channel)
                require(roundtrip.pixels[3 * pixel + channel] == gray[pixel], "PNG roundtrip pixel mismatch");
        // Confirm the file is encoded as an actual grayscale PNG (IHDR color type 0).
        std::ifstream png(output, std::ios::binary);
        unsigned char header[26]{};
        png.read(reinterpret_cast<char*>(header), sizeof(header));
        require(png && header[0] == 137 && header[1] == 'P' && header[24] == 8 && header[25] == 0,
                "Expected 8-bit single-channel PNG");
        png.close();
        expect_error([&] { image::save_grayscale_png(output, 3, 2, image::Bytes(6, 0)); }, "Existing file replaced");
        require(image::load_rgb(output).pixels == roundtrip.pixels, "Existing file changed after refusal");
        expect_error([&] { image::save_grayscale_png(scratch.path / "wrong.png", 3, 2, {1}); }, "Wrong buffer accepted");
        expect_error([&] { image::save_grayscale_png(scratch.path / "wrong.jpg", 3, 2, gray); }, "Wrong extension accepted");
        expect_error([&] { image::save_grayscale_png(scratch.path / "zero.png", 0, 2, {}); }, "Zero dimension accepted");
        expect_error([&] { image::save_grayscale_png(scratch.path / "huge.png", 16385, 1, {}); }, "Huge dimension accepted");
        expect_error([&] { image::save_grayscale_png(scratch.path / "missing" / "out.png", 3, 2, gray); }, "Missing parent accepted");
        expect_error([&] { image::load_rgb(scratch.path / "missing.png"); }, "Missing input accepted");
        const auto invalid = scratch.path / "invalid.png";
        { std::ofstream file(invalid, std::ios::binary); file << "not an image"; }
        expect_error([&] { image::load_rgb(invalid); }, "Corrupt input accepted");
        const auto empty = scratch.path / "empty.png";
        { std::ofstream file(empty, std::ios::binary); }
        expect_error([&] { image::load_rgb(empty); }, "Empty input accepted");
        // A complete PNG header with missing pixel data must fail after header probing.
        const auto truncated = scratch.path / "truncated.png";
        std::ifstream source(fixtures / "rgb.png", std::ios::binary);
        char prefix[33]{};
        source.read(prefix, sizeof(prefix));
        { std::ofstream file(truncated, std::ios::binary); file.write(prefix, sizeof(prefix)); }
        expect_error([&] { image::load_rgb(truncated); }, "Truncated PNG accepted");
        std::cout << "PASS: PNG/JPEG/BMP decoding, alpha handling, PNG roundtrip, Unicode/spaced paths, "
                     "overwrite protection, and invalid image/output errors.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
