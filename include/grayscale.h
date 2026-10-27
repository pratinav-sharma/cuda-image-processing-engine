#pragma once
#include <cstdint>
#include <vector>

namespace image {
using Bytes = std::vector<std::uint8_t>;
// Packed RGB8 input, one grayscale byte per pixel. Reject incomplete RGB pixels.
Bytes grayscale_cpu(const Bytes& rgb);
Bytes grayscale_gpu(const Bytes& rgb, unsigned threads_per_block = 256);
// The active device and compiled kernel both impose a maximum launch size.
unsigned max_grayscale_threads_per_block();
// Queries CUDA device 0; throws if there is no usable GPU. Never falls back to CPU.
void print_device_info();
}
