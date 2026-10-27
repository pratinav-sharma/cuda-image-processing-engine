#pragma once
#include "grayscale.h"

namespace image {
// Nonempty grayscale8, <=16384 per dimension, <=64 Mi pixels; size must match.
void validate_blur_input(const Bytes& gray, int width, int height);
// 3x3 mean, clamp-to-edge sampling, nearest integer: (sum + 4) / 9.
Bytes box_blur_cpu(const Bytes& gray, int width, int height);
}
