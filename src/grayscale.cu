#include "grayscale.h"
#include <cuda_runtime.h>
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
Bytes grayscale_gpu(const Bytes& rgb) {
    if (rgb.size() % 3 != 0) throw std::invalid_argument("RGB input must contain complete pixels");
    Bytes gray(rgb.size() / 3);
    if (gray.empty()) return gray;
    DeviceBuffer input(rgb.size()), output(gray.size());
    check(cudaMemcpy(input.data, rgb.data(), rgb.size(), cudaMemcpyHostToDevice), "copy RGB to GPU");
    constexpr unsigned threads = 256;
    const auto blocks = static_cast<unsigned>((gray.size() + threads - 1) / threads);
    rgb_to_gray<<<blocks, threads>>>(input.data, output.data, gray.size());
    check(cudaGetLastError(), "rgb_to_gray launch");
    check(cudaDeviceSynchronize(), "rgb_to_gray execution");
    check(cudaMemcpy(gray.data(), output.data, gray.size(), cudaMemcpyDeviceToHost), "copy grayscale to CPU");
    return gray;
}
}
