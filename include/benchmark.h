#pragma once
#include "grayscale.h"
#include <vector>

namespace image {
struct BenchmarkOptions {
    int iterations = 50;
    int warmup = 5;
};
struct TimingSummary {
    double min_ms;
    double median_ms;
    double p95_ms;
    double max_ms;
};
struct KernelSamples {
    std::vector<double> milliseconds;
    Bytes output;
};
struct BenchmarkResult {
    std::vector<double> cpu_ms;
    std::vector<double> gpu_total_ms;
    std::vector<double> gpu_kernel_ms;
    Bytes grayscale;
};
void validate_benchmark_options(const BenchmarkOptions& options);
TimingSummary summarize_timings(const std::vector<double>& milliseconds);
// CPU and full-GPU wall times include their existing conversion APIs' allocations.
// Loading, output validation, image saving, and warmups are outside those timers.
BenchmarkResult benchmark_grayscale(const Bytes& rgb, const BenchmarkOptions& options);
// Reuses device buffers and resident RGB; CUDA events bracket each kernel launch.
KernelSamples measure_grayscale_kernel(const Bytes& rgb, const BenchmarkOptions& options);
}
