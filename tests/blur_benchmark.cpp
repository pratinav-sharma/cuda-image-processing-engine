#include "blur_benchmark.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        image::BenchmarkOptions options{3, 1, 256};
        for (const auto kernel : {image::BlurKernel::Global, image::BlurKernel::Shared}) {
            for (const int width : {1, 17}) {
                constexpr int height = 19;
                image::Bytes input(static_cast<std::size_t>(width) * height);
                for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<std::uint8_t>(i * 71u);
                const auto result = image::benchmark_box_blur(input, width, height, kernel, options);
                if (result.cpu_ms.size() != 3 || result.gpu_total_ms.size() != 3 || result.gpu_kernel_ms.size() != 3 ||
                    result.grayscale != image::box_blur_cpu(input, width, height))
                    throw std::runtime_error("Incorrect blur benchmark samples/output");
            }
        }
        options.threads_per_block = 128;
        bool rejected = false;
        try { image::validate_blur_benchmark_options(options); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Unsupported blur block shape accepted");
        options = {0, 1, 256};
        rejected = false;
        try { image::validate_blur_benchmark_options(options); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Invalid iteration count accepted");
        std::cout << "PASS: blur benchmark options, counts and GPU output across both kernels and partial tiles.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
