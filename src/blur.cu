#include "blur.h"
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>

namespace {
constexpr int tile_size = 16;
void check(cudaError_t status, const char* operation) {
    if (status != cudaSuccess)
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
}
struct DeviceBuffer {
    std::uint8_t* data = nullptr;
    explicit DeviceBuffer(std::size_t bytes) {
        check(cudaMalloc(reinterpret_cast<void**>(&data), bytes), "allocate blur buffer");
    }
    ~DeviceBuffer() { if (data) cudaFree(data); }
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
};
__device__ int clamp_coordinate(int coordinate, int extent) {
    return max(0, min(coordinate, extent - 1));
}
__global__ void box_blur_global(const std::uint8_t* input, std::uint8_t* output, int width, int height) {
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (x >= width || y >= height) return;
    unsigned sum = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        const int row = clamp_coordinate(y + dy, height);
        for (int dx = -1; dx <= 1; ++dx) {
            const int column = clamp_coordinate(x + dx, width);
            sum += input[static_cast<std::size_t>(row) * width + column];
        }
    }
    output[static_cast<std::size_t>(y) * width + x] = static_cast<std::uint8_t>((sum + 4u) / 9u);
}
void launch_blur(const std::uint8_t* input, std::uint8_t* output, int width, int height, image::BlurKernel kernel) {
    if (kernel != image::BlurKernel::Global) throw std::invalid_argument("Unknown blur kernel");
    const dim3 block(tile_size, tile_size);
    const dim3 grid((width + tile_size - 1) / tile_size, (height + tile_size - 1) / tile_size);
    box_blur_global<<<grid, block>>>(input, output, width, height);
    check(cudaGetLastError(), "box blur launch");
}
}
namespace image {
Bytes box_blur_gpu(const Bytes& gray, int width, int height, BlurKernel kernel) {
    validate_blur_input(gray, width, height);
    if (kernel != BlurKernel::Global) throw std::invalid_argument("Unknown blur kernel");
    Bytes output(gray.size());
    DeviceBuffer input_device(gray.size()), output_device(gray.size());
    check(cudaMemcpy(input_device.data, gray.data(), gray.size(), cudaMemcpyHostToDevice), "upload blur input");
    launch_blur(input_device.data, output_device.data, width, height, kernel);
    check(cudaDeviceSynchronize(), "box blur execution");
    check(cudaMemcpy(output.data(), output_device.data, output.size(), cudaMemcpyDeviceToHost), "download blur output");
    return output;
}
}
