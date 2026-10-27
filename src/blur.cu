#include "blur.h"
#include "blur_benchmark.h"
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
struct Event {
    cudaEvent_t value{};
    Event() { check(cudaEventCreate(&value), "create blur event"); }
    ~Event() { cudaEventDestroy(value); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
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
__global__ void box_blur_shared(const std::uint8_t* input, std::uint8_t* output, int width, int height) {
    // One-pixel halo around a 16x16 output tile: 18x18 input samples.
    constexpr int side = tile_size + 2;
    __shared__ std::uint8_t tile[side][side];
    const int tx = static_cast<int>(threadIdx.x), ty = static_cast<int>(threadIdx.y);
    const int origin_x = static_cast<int>(blockIdx.x) * tile_size;
    const int origin_y = static_cast<int>(blockIdx.y) * tile_size;
    const int lane = ty * tile_size + tx;
    for (int index = lane; index < side * side; index += tile_size * tile_size) {
        const int tile_y = index / side, tile_x = index % side;
        const int row = clamp_coordinate(origin_y + tile_y - 1, height);
        const int column = clamp_coordinate(origin_x + tile_x - 1, width);
        tile[tile_y][tile_x] = input[static_cast<std::size_t>(row) * width + column];
    }
    // Every thread participates, including threads outside a partial image tile.
    // Returning before this barrier would leave neighbors' tile data incomplete.
    __syncthreads();
    const int x = origin_x + tx, y = origin_y + ty;
    if (x >= width || y >= height) return;
    unsigned sum = 0;
    for (int dy = 0; dy < 3; ++dy)
        for (int dx = 0; dx < 3; ++dx) sum += tile[ty + dy][tx + dx];
    output[static_cast<std::size_t>(y) * width + x] = static_cast<std::uint8_t>((sum + 4u) / 9u);
}
void validate_kernel(image::BlurKernel kernel) {
    if (kernel != image::BlurKernel::Global && kernel != image::BlurKernel::Shared)
        throw std::invalid_argument("Unknown blur kernel");
}
void launch_blur(const std::uint8_t* input, std::uint8_t* output, int width, int height, image::BlurKernel kernel) {
    validate_kernel(kernel);
    const dim3 block(tile_size, tile_size);
    const dim3 grid((width + tile_size - 1) / tile_size, (height + tile_size - 1) / tile_size);
    if (kernel == image::BlurKernel::Global)
        box_blur_global<<<grid, block>>>(input, output, width, height);
    else
        box_blur_shared<<<grid, block>>>(input, output, width, height);
    check(cudaGetLastError(), "box blur launch");
}
}
namespace image {
Bytes box_blur_gpu(const Bytes& gray, int width, int height, BlurKernel kernel) {
    validate_blur_input(gray, width, height);
    validate_kernel(kernel);
    Bytes output(gray.size());
    DeviceBuffer input_device(gray.size()), output_device(gray.size());
    check(cudaMemcpy(input_device.data, gray.data(), gray.size(), cudaMemcpyHostToDevice), "upload blur input");
    launch_blur(input_device.data, output_device.data, width, height, kernel);
    check(cudaDeviceSynchronize(), "box blur execution");
    check(cudaMemcpy(output.data(), output_device.data, output.size(), cudaMemcpyDeviceToHost), "download blur output");
    return output;
}
KernelSamples measure_box_blur_kernel(const Bytes& gray, int width, int height, BlurKernel kernel,
                                     const BenchmarkOptions& options) {
    validate_blur_input(gray, width, height);
    validate_blur_benchmark_options(options);
    validate_kernel(kernel);
    KernelSamples result;
    result.output.resize(gray.size());
    result.milliseconds.reserve(options.iterations);
    DeviceBuffer input_device(gray.size()), output_device(gray.size());
    check(cudaMemcpy(input_device.data, gray.data(), gray.size(), cudaMemcpyHostToDevice), "upload resident blur input");
    const auto launch = [&] { launch_blur(input_device.data, output_device.data, width, height, kernel); };
    for (int i = 0; i < options.warmup; ++i) launch();
    check(cudaDeviceSynchronize(), "synchronize blur warmups");
    Event start, stop;
    for (int i = 0; i < options.iterations; ++i) {
        check(cudaEventRecord(start.value), "record blur start");
        launch();
        check(cudaEventRecord(stop.value), "record blur stop");
        check(cudaEventSynchronize(stop.value), "wait for blur stop");
        float milliseconds = 0;
        check(cudaEventElapsedTime(&milliseconds, start.value, stop.value), "measure blur kernel");
        result.milliseconds.push_back(milliseconds);
    }
    check(cudaMemcpy(result.output.data(), output_device.data, result.output.size(), cudaMemcpyDeviceToHost), "validate resident blur output");
    return result;
}
}
