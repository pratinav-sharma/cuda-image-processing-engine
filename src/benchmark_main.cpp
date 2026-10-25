#include "benchmark.h"
#include "image_io.h"
#include <charconv>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>

namespace {
void usage() {
    std::cout << "Usage: image_benchmark input-image [--iterations 50] [--warmup 5]\n"
                 "                       [--csv results.csv] [--output grayscale.png]\n"
                 "Compare CPU, full GPU conversion, and resident GPU kernel timing.\n"
                 "Input: PNG/JPEG/BMP. Outputs must be new files; nothing is overwritten.\n";
}
int parse_count(const std::string& value, int maximum, const std::string& option) {
    int count = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || count < 1 || count > maximum)
        throw std::invalid_argument(option + " must be an integer between 1 and " + std::to_string(maximum));
    return count;
}
void check_new_output(const std::filesystem::path& path, const char* extension) {
    auto suffix = path.extension().u8string();
    for (auto& c : suffix) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    if (suffix != extension) throw std::invalid_argument(std::string("Output must end in ") + extension);
    if (std::filesystem::exists(path)) throw std::runtime_error("Output already exists: " + path.u8string());
    if (path.has_parent_path() && !std::filesystem::is_directory(path.parent_path()))
        throw std::runtime_error("Output parent directory does not exist: " + path.u8string());
}
std::string csv_quote(const std::string& value) {
    std::string quoted = "\"";
    for (const char c : value) { if (c == '"') quoted += '"'; quoted += c; }
    return quoted + '"';
}
void save_csv(const std::filesystem::path& path, const std::filesystem::path& input,
              const image::RgbImage& loaded, const image::BenchmarkOptions& options,
              const image::BenchmarkResult& result) {
    check_new_output(path, ".csv");
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot create benchmark CSV: " + path.u8string());
    output.imbue(std::locale::classic());
    output << std::setprecision(9)
           << "input,width,height,warmup,build,method,iteration,milliseconds\n";
    const auto write = [&](const char* method, const std::vector<double>& samples) {
        for (std::size_t i = 0; i < samples.size(); ++i)
            output << csv_quote(input.u8string()) << ',' << loaded.width << ',' << loaded.height << ','
                   << options.warmup << ',' << IMAGE_BUILD_CONFIG << ',' << method << ',' << i + 1 << ',' << samples[i] << '\n';
    };
    write("cpu", result.cpu_ms);
    write("gpu_full", result.gpu_total_ms);
    write("gpu_kernel", result.gpu_kernel_ms);
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw std::runtime_error("Failed to write benchmark CSV: " + path.u8string());
    }
}
void print_row(const char* label, const image::TimingSummary& timing) {
    std::cout << std::left << std::setw(36) << label << std::right << std::setw(12) << timing.median_ms
              << std::setw(12) << timing.p95_ms << std::setw(12) << timing.min_ms << std::setw(12) << timing.max_ms << '\n';
}
void print_ratio(const char* label, double cpu, double gpu) {
    std::cout << label;
    if (gpu > 0) std::cout << cpu / gpu << "x\n";
    else std::cout << "n/a (below timer resolution)\n";
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        if (argc == 2 && std::filesystem::path(argv[1]) == "--help") { usage(); return 0; }
        if (argc < 2) { usage(); return 1; }
        const std::filesystem::path input(argv[1]);
        std::filesystem::path csv_path, output_path;
        image::BenchmarkOptions options;
        for (int i = 2; i < argc; ++i) {
            const auto option = std::filesystem::path(argv[i]).u8string();
            if (option != "--iterations" && option != "--warmup" && option != "--csv" && option != "--output")
                throw std::invalid_argument("Unknown option: " + option);
            if (++i == argc) throw std::invalid_argument("Missing value for " + option);
            if (option == "--iterations") options.iterations = parse_count(std::filesystem::path(argv[i]).u8string(), 10000, option);
            else if (option == "--warmup") options.warmup = parse_count(std::filesystem::path(argv[i]).u8string(), 1000, option);
            else if (option == "--csv") csv_path = std::filesystem::path(argv[i]);
            else output_path = std::filesystem::path(argv[i]);
        }
        if (!csv_path.empty()) check_new_output(csv_path, ".csv");
        if (!output_path.empty()) check_new_output(output_path, ".png");
        using Clock = std::chrono::steady_clock;
        const auto load_start = Clock::now();
        const auto loaded = image::load_rgb(input);
        const double load_ms = std::chrono::duration<double, std::milli>(Clock::now() - load_start).count();
        image::print_device_info();
        std::cout << "Input: " << input.u8string() << "\nImage: " << loaded.width << 'x' << loaded.height
                  << " (" << loaded.pixels.size() / 3 << " pixels)\nBuild: " << IMAGE_BUILD_CONFIG
                  << "; CPU baseline: single thread\nWarmups: " << options.warmup
                  << "; measured samples per method: " << options.iterations << '\n';
        const auto result = image::benchmark_grayscale(loaded.pixels, options);
        const auto cpu = image::summarize_timings(result.cpu_ms);
        const auto full = image::summarize_timings(result.gpu_total_ms);
        const auto kernel = image::summarize_timings(result.gpu_kernel_ms);
        std::cout << std::fixed << std::setprecision(4) << '\n'
                  << std::left << std::setw(36) << "Conversion timing (milliseconds)"
                  << std::right << std::setw(12) << "Median" << std::setw(12) << "P95"
                  << std::setw(12) << "Min" << std::setw(12) << "Max" << '\n';
        print_row("CPU function (host -> host)", cpu);
        print_row("GPU full function (host -> host)", full);
        print_row("GPU kernel (resident device data)", kernel);
        std::cout << '\n';
        print_ratio("Full GPU conversion speedup (CPU/GPU): ", cpu.median_ms, full.median_ms);
        print_ratio("Kernel-only timing ratio (CPU/kernel): ", cpu.median_ms, kernel.median_ms);
        std::cout << "A ratio above 1 favors GPU; below 1 favors CPU. Kernel-only is not full conversion speedup.\n"
                  << "Full GPU includes output/device allocation, upload, kernel, synchronization, download, and device frees.\n"
                  << "Kernel events exclude allocations/transfers; buffers stay on the device and output is checked afterward.\n"
                  << "CPU/GPU full measurements alternate order; CPU/GPU output checks are outside timers.\n"
                  << "Image decode (once, excluded from conversion timings): " << load_ms << " ms\n"
                  << "PASS: measured CPU/GPU full outputs and final resident-kernel output match the CPU reference.\n";
        if (!output_path.empty()) {
            const auto save_start = Clock::now();
            image::save_grayscale_png(output_path, loaded.width, loaded.height, result.grayscale);
            const double save_ms = std::chrono::duration<double, std::milli>(Clock::now() - save_start).count();
            std::cout << "PNG encode/write (once, excluded): " << save_ms << " ms; saved " << output_path.u8string() << '\n';
        }
        if (!csv_path.empty()) {
            save_csv(csv_path, input, loaded, options, result);
            std::cout << "Raw samples saved: " << csv_path.u8string() << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
