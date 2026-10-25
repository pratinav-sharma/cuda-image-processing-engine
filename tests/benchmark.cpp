#include "benchmark.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Operation>
void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Invalid benchmark input was accepted");
}
void check_samples(const std::vector<double>& samples, std::size_t count) {
    require(samples.size() == count, "Wrong measured sample count (warmups must not be counted)");
    for (const auto value : samples) require(std::isfinite(value) && value >= 0, "Invalid elapsed time");
    const auto stats = image::summarize_timings(samples);
    require(stats.min_ms <= stats.median_ms && stats.median_ms <= stats.p95_ms && stats.p95_ms <= stats.max_ms,
            "Timing summary is not ordered");
}
}

int main() {
    try {
        const auto even = image::summarize_timings({4,1,3,2});
        require(even.min_ms == 1 && even.median_ms == 2.5 && even.p95_ms == 4 && even.max_ms == 4,
                "Even-count median or percentile mismatch");
        const auto odd = image::summarize_timings({9,1,3});
        require(odd.median_ms == 3 && odd.p95_ms == 9, "Odd-count median or percentile mismatch");
        const auto single = image::summarize_timings({0});
        require(single.min_ms == 0 && single.median_ms == 0 && single.p95_ms == 0 && single.max_ms == 0,
                "Single sample or zero-resolution timing rejected");
        std::vector<double> sequence;
        for (int i = 1; i <= 20; ++i) sequence.push_back(i);
        require(image::summarize_timings(sequence).p95_ms == 19, "P95 must use nearest-rank percentile");
        rejects([] { image::summarize_timings({}); });
        rejects([] { image::summarize_timings({-1}); });
        rejects([] { image::summarize_timings({std::numeric_limits<double>::infinity()}); });
        rejects([] { image::summarize_timings({std::numeric_limits<double>::quiet_NaN()}); });
        rejects([] { image::validate_benchmark_options({0,1}); });
        rejects([] { image::validate_benchmark_options({10001,1}); });
        rejects([] { image::validate_benchmark_options({1,0}); });
        rejects([] { image::validate_benchmark_options({1,1001}); });
        rejects([] { image::benchmark_grayscale({}, {1,1}); });
        rejects([] { image::benchmark_grayscale({1,2}, {1,1}); });
        rejects([] { image::measure_grayscale_kernel({}, {1,1}); });
        rejects([] { image::measure_grayscale_kernel({1,2}, {1,1}); });
        image::validate_benchmark_options({10000,1000});

        // Exercise both the one-thread case and a partial final CUDA block.
        for (const std::size_t pixels : {std::size_t{1}, std::size_t{257}}) {
            image::Bytes rgb(pixels * 3);
            for (std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<std::uint8_t>((i * 37) % 256);
            const auto result = image::benchmark_grayscale(rgb, {3,2});
            require(result.grayscale == image::grayscale_cpu(rgb), "Timed output differs from CPU reference");
            check_samples(result.cpu_ms, 3);
            check_samples(result.gpu_total_ms, 3);
            check_samples(result.gpu_kernel_ms, 3);
        }
        std::cout << "PASS: timing summaries, option validation, exact sample counts, and GPU benchmark correctness.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
