#include "blur_benchmark.h"
#include <chrono>
#include <stdexcept>
#include <utility>

namespace image {
void validate_blur_benchmark_options(const BenchmarkOptions& options) {
    validate_benchmark_options(options);
    if (options.threads_per_block != 256)
        throw std::invalid_argument("Blur currently uses 16x16 blocks (256 threads)");
}
BenchmarkResult benchmark_box_blur(const Bytes& gray, int width, int height, BlurKernel kernel,
                                   const BenchmarkOptions& options) {
    validate_blur_benchmark_options(options);
    const auto reference = box_blur_cpu(gray, width, height);
    for (int i = 0; i < options.warmup; ++i) {
        if (box_blur_cpu(gray, width, height) != reference || box_blur_gpu(gray, width, height, kernel) != reference)
            throw std::runtime_error("Blur warmup output mismatch");
    }
    BenchmarkResult result;
    result.cpu_ms.reserve(options.iterations);
    result.gpu_total_ms.reserve(options.iterations);
    using Clock = std::chrono::steady_clock;
    const auto cpu_sample = [&] {
        const auto start = Clock::now();
        const auto output = box_blur_cpu(gray, width, height);
        const auto stop = Clock::now();
        result.cpu_ms.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
        if (output != reference) throw std::runtime_error("Measured CPU blur mismatch");
    };
    const auto gpu_sample = [&] {
        const auto start = Clock::now();
        auto output = box_blur_gpu(gray, width, height, kernel);
        const auto stop = Clock::now();
        result.gpu_total_ms.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
        if (output != reference) throw std::runtime_error("Measured GPU blur mismatch");
        result.grayscale = std::move(output);
    };
    for (int i = 0; i < options.iterations; ++i) {
        if (i % 2 == 0) { cpu_sample(); gpu_sample(); }
        else { gpu_sample(); cpu_sample(); }
    }
    auto kernels = measure_box_blur_kernel(gray, width, height, kernel, options);
    if (kernels.output != reference) throw std::runtime_error("Resident blur output mismatch");
    result.gpu_kernel_ms = std::move(kernels.milliseconds);
    return result;
}
}
