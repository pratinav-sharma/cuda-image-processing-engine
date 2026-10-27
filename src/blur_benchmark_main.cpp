#include "blur_benchmark.h"
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
    std::cout << "Usage: blur_benchmark input-image [--kernel global|shared] [--iterations 50]\n"
                 "                      [--warmup 5] [--csv new.csv] [--output new.png]\n"
                 "Compare CPU, full GPU and resident GPU 3x3 grayscale box blur.\n"
                 "Decode and CPU grayscale preparation are outside blur timers.\n"
                 "Clamp-to-edge, nearest integer rounding, fixed 16x16 CUDA blocks.\n";
}
int count(const std::string& text, int maximum, const std::string& option) {
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < 1 || value > maximum)
        throw std::invalid_argument(option + " must be between 1 and " + std::to_string(maximum));
    return value;
}
void new_output(const std::filesystem::path& path, const char* extension) {
    auto suffix = path.extension().u8string();
    for (auto& c : suffix) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    if (suffix != extension) throw std::invalid_argument(std::string("Output must end in ") + extension);
    if (std::filesystem::exists(path)) throw std::runtime_error("Output already exists: " + path.u8string());
    if (path.has_parent_path() && !std::filesystem::is_directory(path.parent_path()))
        throw std::runtime_error("Output parent directory does not exist");
}
std::string quote(const std::string& text) {
    std::string result = "\"";
    for (const char c : text) { if (c == '"') result += '"'; result += c; }
    return result + '"';
}
void save_csv(const std::filesystem::path& path, const std::filesystem::path& input, int width, int height,
              const char* kernel, const image::BenchmarkOptions& options, const image::BenchmarkResult& result) {
    new_output(path, ".csv");
    std::ofstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot create blur CSV");
    stream.imbue(std::locale::classic());
    stream << std::setprecision(9)
           << "input,width,height,kernel,warmup,block_x,block_y,grid_x,grid_y,build,method,iteration,milliseconds\n";
    const auto rows = [&](const char* method, const std::vector<double>& samples) {
        for (std::size_t i = 0; i < samples.size(); ++i)
            stream << quote(input.u8string()) << ',' << width << ',' << height << ',' << kernel << ','
                   << options.warmup << ",16,16," << (width + 15) / 16 << ',' << (height + 15) / 16
                   << ',' << IMAGE_BUILD_CONFIG << ',' << method << ',' << i + 1 << ',' << samples[i] << '\n';
    };
    rows("cpu", result.cpu_ms); rows("gpu_full", result.gpu_total_ms); rows("gpu_kernel", result.gpu_kernel_ms);
    stream.close();
    if (!stream) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw std::runtime_error("Failed to write blur CSV");
    }
}
void row(const char* label, const std::vector<double>& values) {
    const auto s = image::summarize_timings(values);
    std::cout << std::left << std::setw(25) << label << std::right << std::setw(12) << s.median_ms
              << std::setw(12) << s.p95_ms << std::setw(12) << s.min_ms << std::setw(12) << s.max_ms << '\n';
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
        std::filesystem::path csv, output;
        auto kernel = image::BlurKernel::Global;
        image::BenchmarkOptions options;
        for (int i = 2; i < argc; ++i) {
            const auto option = std::filesystem::path(argv[i]).u8string();
            if (option != "--kernel" && option != "--iterations" && option != "--warmup" && option != "--csv" && option != "--output")
                throw std::invalid_argument("Unknown option: " + option);
            if (++i == argc) throw std::invalid_argument("Missing value for " + option);
            const auto value = std::filesystem::path(argv[i]).u8string();
            if (option == "--iterations") options.iterations = count(value, 10000, option);
            else if (option == "--warmup") options.warmup = count(value, 1000, option);
            else if (option == "--csv") csv = std::filesystem::path(argv[i]);
            else if (option == "--output") output = std::filesystem::path(argv[i]);
            else if (value == "global") kernel = image::BlurKernel::Global;
            else if (value == "shared") kernel = image::BlurKernel::Shared;
            else throw std::invalid_argument("--kernel must be global or shared");
        }
        if (!csv.empty()) new_output(csv, ".csv");
        if (!output.empty()) new_output(output, ".png");
        const auto start = std::chrono::steady_clock::now();
        const auto loaded = image::load_rgb(input);
        const auto gray = image::grayscale_cpu(loaded.pixels);
        const double prepare_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        image::print_device_info();
        const char* name = kernel == image::BlurKernel::Global ? "global" : "shared";
        std::cout << "Image: " << loaded.width << 'x' << loaded.height << "; blur kernel: " << name
                  << "; build: " << IMAGE_BUILD_CONFIG << "\nBlock: 16x16; grid: " << (loaded.width + 15) / 16
                  << 'x' << (loaded.height + 15) / 16 << "; clamp-to-edge; round (sum+4)/9\n"
                  << "Warmups: " << options.warmup << "; samples per method: " << options.iterations
                  << "; CPU baseline: single thread\n";
        const auto result = image::benchmark_box_blur(gray, loaded.width, loaded.height, kernel, options);
        std::cout << std::fixed << std::setprecision(4) << std::left << std::setw(25) << "Blur timing (ms)"
                  << std::right << std::setw(12) << "Median" << std::setw(12) << "P95" << std::setw(12) << "Min"
                  << std::setw(12) << "Max" << '\n';
        row("CPU full", result.cpu_ms); row("GPU full", result.gpu_total_ms); row("GPU resident kernel", result.gpu_kernel_ms);
        const double cpu = image::summarize_timings(result.cpu_ms).median_ms;
        const double gpu = image::summarize_timings(result.gpu_total_ms).median_ms;
        std::cout << "Full blur speedup (CPU/GPU): ";
        if (gpu > 0) std::cout << cpu / gpu << "x\n"; else std::cout << "n/a\n";
        std::cout << "GPU full includes output/device allocations, upload, launch, wait, download, and device frees.\n"
                  << "Resident events exclude allocation/transfers. Warmups/checks are outside timers.\n"
                  << "Decode and CPU grayscale preparation (once, excluded): " << prepare_ms << " ms\n"
                  << "This measures blur on prepared grayscale, not the whole RGB-to-PNG pipeline.\n"
                  << "PASS: every measured full output and final resident output match CPU blur.\n";
        if (!output.empty()) {
            image::save_grayscale_png(output, loaded.width, loaded.height, result.grayscale);
            std::cout << "Saved blurred PNG: " << output.u8string() << '\n';
        }
        if (!csv.empty()) { save_csv(csv, input, loaded.width, loaded.height, name, options, result); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
