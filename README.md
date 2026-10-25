# cuda-image-processing-engine
GPU-accelerated image processing in CUDA C++, featuring custom kernels, CPU/GPU benchmarks, and memory optimization experiments.

## Minimal CUDA C++ project

The starter project converts packed RGB8 images to grayscale on a real CUDA GPU.
The independent CPU reference and GPU kernel use integer luminance
`(77 * R + 150 * G + 29 * B + 128) / 256`, rounded down after adding 128.
No external image libraries or ML frameworks are required. Benchmarking and memory
optimization remain future work; the demo currently verifies correctness.

Targets:

- `image_processing`: C++ CPU reference and CUDA kernel library.
- `image_demo`: generates a 1920x1080 image, executes the kernel, and checks every pixel.
- `image_correctness`: known colors, empty/invalid input, and 28 image comparisons
  across seven sizes and four patterns (black, white, alternating, seeded random).
  Includes 1, 255, 256, and 257 pixels and non-square images to test block boundaries.

CUDA allocation, copies, launch, and synchronization are checked. Missing GPUs and
CUDA errors produce a nonzero exit status; there is no CPU execution fallback.

## Windows setup

Use 64-bit Windows, an NVIDIA GPU and driver, CUDA Toolkit, Visual Studio 2022
Build Tools with MSVC x64/x86 and Windows SDK, CMake >= 3.24, and Ninja.
The Visual Studio C++ CMake component supplies both CMake and Ninja.
Follow the [NVIDIA compiler support table](https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/index.html)
when changing compiler or toolkit versions. Do not bypass `nvcc` compiler checks.

The setup used these package commands (PowerShell):

```powershell
winget install --id Git.Git --exact --source winget --scope user --silent --accept-package-agreements --accept-source-agreements --disable-interactivity
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --silent --accept-package-agreements --accept-source-agreements --disable-interactivity --override '--quiet --wait --norestart --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project'
winget install --id Nvidia.CUDA --exact --version 13.4 --source winget --silent --accept-package-agreements --accept-source-agreements --disable-interactivity --override '-s nvcc_13.4 cudart_13.4 crt_13.4 nvvm_13.4 thrust_13.4 -n'
```

WinGet verifies package SHA256 hashes and uses HTTPS. These commands preserve TLS
and package verification. Administrator prompts may require user interaction.
The CUDA 13.4 installer does not include a graphics driver; `-n` and `--norestart`
prevent automatic reboots. Obtain approval before separate driver changes or reboots.

From the repository root, persist tool discovery once:

```powershell
.\scripts\Set-UserEnvironment.ps1
```

This adds CUDA/CMake/Ninja directories to the user PATH only where absent, saves
`CUDA_PATH`, and backs up previous environment values under
`$env:LOCALAPPDATA\cuda-image-processing-engine`. Existing PATH entries are retained.
Open a new terminal after changing persistent variables. Initialize MSVC for each
development shell using Microsoft's Developer PowerShell launcher, as below;
compiler-specific INCLUDE/LIB paths are intentionally initialized per shell.

## Build, test, and run

In PowerShell, from the repository root:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
nvidia-smi
nvcc --version
cl
cmake --version
ninja --version
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=89
cmake --build build --parallel
ctest --test-dir build --output-on-failure --verbose
.\build\image_demo.exe
```

Architecture `89` targets this machine's RTX 4060 (Ada, compute capability 8.9).
For another GPU omit the architecture argument to use `native`, or supply its
compute capability. Use a separate build directory when changing toolchains.
`cl` without input prints its version and may return a nonzero exit code; the
CMake build performs the actual compiler compatibility check.

For Linux with compatible GCC, CUDA, CMake and Ninja already installed:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure --verbose
./build/image_demo
```

Linux/WSL/container commands are provided for portability and require independent
verification. In WSL, use the Windows host GPU driver and a WSL-compatible toolkit;
in a container, GPU access must be passed through by the host runtime.

## Verified machine and results (2026-10-25)

The checkout is at
`C:\Users\prati\OneDrive\Documents\ChatGPT\New project\cuda-image-processing-engine`.
The original repository contained only this README; its description was retained.
No `AGENTS.md` was found in the checkout or its parent directories. The enclosing
workspace's existing Git metadata was preserved by cloning into this subdirectory.

| Component | Detected version / configuration |
| --- | --- |
| OS | Native Windows 11 Home, 26H2, build 10.0.26300, x64 |
| GPU | NVIDIA GeForce RTX 4060, 8188 MiB reported by `nvidia-smi`, compute capability 8.9 |
| NVIDIA driver | 621.01, WDDM; retained |
| CUDA Toolkit | 13.4.1; `nvcc` 13.4.59 |
| CUDA runtime / driver API | Both report 13050 (13.5) at execution |
| Visual Studio Build Tools | 2022, version 17.14.42 |
| MSVC compiler | 19.44.35229, x64; toolset directory 14.44.35207 |
| Windows SDK | 10.0.26100.0 |
| CMake | 3.31.6-msvc6 |
| Ninja | 1.12.1 |
| Git | 2.56.0.windows.3 |

The runtime version can differ from the build toolkit on Windows starting with
CUDA 13.0: it reports the runtime packaged with the display driver. See
[NVIDIA runtime version documentation](https://docs.nvidia.com/cuda/cuda-runtime-api/cuda_runtime_api/group__CUDART____VERSION.html).
The installed toolkit headers report `CUDART_VERSION=13040`; the driver-provided
runtime reported 13050 during both executions.

Successful checks:

- `nvidia-smi` GPU access before and after setup; driver remains 621.01.
- WinGet package hash verification; CUDA installer SHA256:
  `632a5912995473cea77b24788715b8d94db17a94cfea102a12f72c3614fe8c7e`.
  Its Authenticode signature was valid and signed by NVIDIA Corporation.
- CPU reference compiled and executed with MSVC, including known-color and input validation checks.
- CMake identified NVIDIA CUDA 13.4.59 with MSVC 19.44.35229 and built the library,
  demo, and correctness executable without an unsupported-compiler override.
- The real CUDA demo synchronized the kernel, copied its results back, and matched
  all 2,073,600 pixels of a 1920x1080 image against the CPU reference.
- CTest: 1/1 test passed, zero failed, including known colors and 28 GPU/CPU image
  comparisons; observed test duration 1.15 seconds.
- Persistent user `CUDA_PATH` and CMake/Ninja PATH entries were saved. CUDA's bin
  was already present in the machine PATH after installation; the prior user PATH
  was verified intact. Previous values are backed up under
  `%LOCALAPPDATA%\cuda-image-processing-engine`.

No driver change or reboot was performed. No external setup blocker remains.
WSL/container passthrough and Linux builds were not run because this is a native
Windows setup. Performance benchmarks and compute-sanitizer runs were not performed.
