#pragma once
#include "benchmark.h"
#include "blur.h"

namespace image {
// Blur uses fixed 16x16 blocks. BenchmarkOptions::threads_per_block must be 256.
void validate_blur_benchmark_options(const BenchmarkOptions& options);
BenchmarkResult benchmark_box_blur(const Bytes& gray, int width, int height, BlurKernel kernel,
                                   const BenchmarkOptions& options);
KernelSamples measure_box_blur_kernel(const Bytes& gray, int width, int height, BlurKernel kernel,
                                     const BenchmarkOptions& options);
}
