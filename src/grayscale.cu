#include "grayscale.h"
#include "benchmark.h"
#include <cuda_runtime.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(cudaError_t status, const char* operation) {
    if (status != cudaSuccess)
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
}
struct DeviceBuffer {
    std::uint8_t* data = nullptr;
    explicit DeviceBuffer(std::size_t bytes) { check(cudaMalloc(reinterpret_cast<void**>(&data), bytes), "cudaMalloc"); }
    ~DeviceBuffer() { if (data) cudaFree(data); }
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
};
struct Event {
    cudaEvent_t value{};
    Event() { check(cudaEventCreate(&value), "cudaEventCreate"); }
    ~Event() { cudaEventDestroy(value); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
};
__global__ void rgb_to_gray(const std::uint8_t* rgb, std::uint8_t* gray, std::size_t pixels) {
    const std::size_t pixel = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (pixel < pixels) {
        const unsigned sum = 77u * rgb[3 * pixel] + 150u * rgb[3 * pixel + 1]
                           + 29u * rgb[3 * pixel + 2] + 128u;
        gray[pixel] = static_cast<std::uint8_t>(sum >> 8);
    }
}
}
namespace image {
void print_device_info() {
    int count = 0;
    check(cudaGetDeviceCount(&count), "cudaGetDeviceCount");
    if (count == 0) throw std::runtime_error("No CUDA GPU available");
    check(cudaSetDevice(0), "cudaSetDevice");
    cudaDeviceProp props{};
    check(cudaGetDeviceProperties(&props, 0), "cudaGetDeviceProperties");
    int driver = 0, runtime = 0;
    check(cudaDriverGetVersion(&driver), "cudaDriverGetVersion");
    check(cudaRuntimeGetVersion(&runtime), "cudaRuntimeGetVersion");
    std::cout << "GPU: " << props.name << ", compute capability " << props.major << '.' << props.minor
              << ", memory " << props.totalGlobalMem / (1024 * 1024) << " MiB\n"
              << "CUDA driver API: " << driver << ", runtime: " << runtime << '\n';
}
unsigned max_grayscale_threads_per_block() {
    int device = 0;
    check(cudaGetDevice(&device), "cudaGetDevice");
    cudaDeviceProp props{};
    check(cudaGetDeviceProperties(&props, device), "query launch limits");
    cudaFuncAttributes attributes{};
    check(cudaFuncGetAttributes(&attributes, rgb_to_gray), "query kernel launch limits");
    return static_cast<unsigned>(std::min({props.maxThreadsPerBlock, props.maxThreadsDim[0], attributes.maxThreadsPerBlock}));
}
Bytes grayscale_gpu(const Bytes& rgb, unsigned threads_per_block) {
    if (threads_per_block < 1 || threads_per_block > 1024)
        throw std::invalid_argument("Threads per block must be between 1 and 1024");
    if (rgb.size() % 3 != 0) throw std::invalid_argument("RGB input must contain complete pixels");
    Bytes gray(rgb.size() / 3);
    if (gray.empty()) return gray;
    if (threads_per_block > max_grayscale_threads_per_block())
        throw std::invalid_argument("Threads per block exceed the active GPU/kernel limit");
    DeviceBuffer input(rgb.size()), output(gray.size());
    check(cudaMemcpy(input.data, rgb.data(), rgb.size(), cudaMemcpyHostToDevice), "copy RGB to GPU");
    const unsigned threads = threads_per_block;
    const auto blocks = static_cast<unsigned>((gray.size() + threads - 1) / threads);
    rgb_to_gray<<<blocks, threads>>>(input.data, output.data, gray.size());
    check(cudaGetLastError(), "rgb_to_gray launch");
    check(cudaDeviceSynchronize(), "rgb_to_gray execution");
    check(cudaMemcpy(gray.data(), output.data, gray.size(), cudaMemcpyDeviceToHost), "copy grayscale to CPU");
    return gray;
}

KernelSamples measure_grayscale_kernel(const Bytes& rgb, const BenchmarkOptions& options) {
    validate_benchmark_options(options);
    if (rgb.empty() || rgb.size() % 3 != 0)
        throw std::invalid_argument("Kernel benchmark requires a nonempty packed RGB image");
    if (options.threads_per_block > max_grayscale_threads_per_block())
        throw std::invalid_argument("Threads per block exceed the active GPU/kernel limit");
    KernelSamples result;
    result.output.resize(rgb.size() / 3);
    result.milliseconds.reserve(options.iterations);
    DeviceBuffer input(rgb.size()), output(result.output.size());
    check(cudaMemcpy(input.data, rgb.data(), rgb.size(), cudaMemcpyHostToDevice), "upload benchmark RGB");
    const unsigned threads = options.threads_per_block;
    const auto blocks = static_cast<unsigned>((result.output.size() + threads - 1) / threads);
    const auto launch = [&] {
        rgb_to_gray<<<blocks, threads>>>(input.data, output.data, result.output.size());
        check(cudaGetLastError(), "benchmark rgb_to_gray launch");
    };
    for (int i = 0; i < options.warmup; ++i) launch();
    check(cudaDeviceSynchronize(), "benchmark warmup synchronization");
    Event start, stop;
    for (int i = 0; i < options.iterations; ++i) {
        check(cudaEventRecord(start.value), "record kernel start");
        launch();
        check(cudaEventRecord(stop.value), "record kernel stop");
        check(cudaEventSynchronize(stop.value), "synchronize kernel stop");
        float milliseconds = 0;
        check(cudaEventElapsedTime(&milliseconds, start.value, stop.value), "measure kernel time");
        result.milliseconds.push_back(milliseconds);
    }
    check(cudaMemcpy(result.output.data(), output.data, result.output.size(), cudaMemcpyDeviceToHost),
          "download benchmark grayscale for validation");
    return result;
}
}
