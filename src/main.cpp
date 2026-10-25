#include "grayscale.h"
#include "image_io.h"
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        if (argc == 2 && std::filesystem::path(argv[1]) == "--help") {
            std::cout << "Usage: image_demo [input-image output.png]\n"
                      << "Load PNG/JPEG/BMP, convert on the GPU, verify against CPU, and save PNG.\n"
                      << "Without arguments, run the generated-image correctness demo.\n"
                      << "Existing output files are preserved; choose a new filename.\n";
            return 0;
        }
        if (argc != 1 && argc != 3) throw std::invalid_argument("Usage: image_demo [input-image output.png]");
        if (argc == 3) {
            const std::filesystem::path input(argv[1]), output(argv[2]);
            if (std::filesystem::exists(output))
                throw std::runtime_error("Output already exists; choose a new filename: " + output.u8string());
            const auto loaded = image::load_rgb(input);
            image::print_device_info();
            const auto gpu = image::grayscale_gpu(loaded.pixels);
            if (gpu != image::grayscale_cpu(loaded.pixels)) throw std::runtime_error("CPU/GPU mismatch");
            image::save_grayscale_png(output, loaded.width, loaded.height, gpu);
            std::cout << "PASS: loaded " << loaded.width << 'x' << loaded.height << " image; all "
                      << gpu.size() << " GPU pixels match the CPU reference.\n"
                      << "Saved grayscale PNG: " << output.u8string() << '\n';
            return 0;
        }
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
