#include "benchmark.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace image {
void validate_benchmark_options(const BenchmarkOptions& options) {
    if (options.iterations < 1 || options.iterations > 10000)
        throw std::invalid_argument("Iterations must be between 1 and 10000");
    if (options.warmup < 1 || options.warmup > 1000)
        throw std::invalid_argument("Warmup count must be between 1 and 1000");
    if (options.threads_per_block < 1 || options.threads_per_block > 1024)
        throw std::invalid_argument("Threads per block must be between 1 and 1024");
}

TimingSummary summarize_timings(const std::vector<double>& milliseconds) {
    if (milliseconds.empty()) throw std::invalid_argument("No timing samples");
    auto sorted = milliseconds;
    for (const double value : sorted)
        if (!std::isfinite(value) || value < 0) throw std::invalid_argument("Invalid timing sample");
    std::sort(sorted.begin(), sorted.end());
    const auto n = sorted.size();
    const double median = n % 2 ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
    const auto p95_index = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(n))) - 1;
    return {sorted.front(), median, sorted[p95_index], sorted.back()};
}

BenchmarkResult benchmark_grayscale(const Bytes& rgb, const BenchmarkOptions& options) {
    validate_benchmark_options(options);
    if (rgb.empty() || rgb.size() % 3 != 0)
        throw std::invalid_argument("Benchmark requires a nonempty packed RGB image");
    const auto reference = grayscale_cpu(rgb);
    for (int i = 0; i < options.warmup; ++i) {
        if (grayscale_cpu(rgb) != reference || grayscale_gpu(rgb, options.threads_per_block) != reference)
            throw std::runtime_error("CPU/GPU mismatch during warmup");
    }
    BenchmarkResult result;
    result.cpu_ms.reserve(options.iterations);
    result.gpu_total_ms.reserve(options.iterations);
    using Clock = std::chrono::steady_clock;
    const auto cpu_sample = [&] {
        const auto start = Clock::now();
        const auto output = grayscale_cpu(rgb);
        const auto stop = Clock::now();
        result.cpu_ms.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
        if (output != reference) throw std::runtime_error("CPU mismatch during measurement");
    };
    const auto gpu_sample = [&] {
        const auto start = Clock::now();
        auto output = grayscale_gpu(rgb, options.threads_per_block); // Includes synchronization and internal buffer frees.
        const auto stop = Clock::now();
        result.gpu_total_ms.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
        if (output != reference) throw std::runtime_error("GPU mismatch during measurement");
        result.grayscale = std::move(output);
    };
    for (int i = 0; i < options.iterations; ++i) {
        // Alternate order to reduce consistent first/second-run bias.
        if (i % 2 == 0) { cpu_sample(); gpu_sample(); }
        else { gpu_sample(); cpu_sample(); }
    }
    auto kernels = measure_grayscale_kernel(rgb, options);
    if (kernels.output != reference) throw std::runtime_error("Resident GPU kernel output mismatch");
    result.gpu_kernel_ms = std::move(kernels.milliseconds);
    return result;
}
}
