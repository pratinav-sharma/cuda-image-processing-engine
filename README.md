# cuda-image-processing-engine
GPU-accelerated image processing in CUDA C++, featuring custom kernels, CPU/GPU benchmarks, and memory optimization experiments.

The [implementation journal](docs/implementation-notes.md) explains our approach
commit by commit, with validation evidence and interview practice questions.

## Minimal CUDA C++ project

The project converts packed RGB8 images to grayscale on a real CUDA GPU and
compares CPU, global-memory GPU, and shared-memory GPU 3x3 blur implementations.
The independent CPU reference and GPU kernel use integer luminance
`(77 * R + 150 * G + 29 * B + 128) / 256`, rounded down after adding 128.
Image file support uses two vendored stb headers; no separate image-library
installation or ML framework is required. The demo verifies correctness; a
separate benchmark compares CPU, full GPU conversion, and resident-kernel timing.

Targets:

- `image_processing`: CPU references, CUDA grayscale/blur kernels, and timing APIs.
- `image_io`: PNG/JPEG/BMP loading and single-channel grayscale PNG writing.
- `image_demo`: loads an image and saves a verified grayscale PNG, or generates a
  1920x1080 image when invoked without arguments.
- `image_benchmark`: benchmarks an actual image, prints median/p95/min/max times,
  and optionally exports every sample as CSV and saves a verified grayscale PNG.
- `blur_benchmark`: compares CPU/full-GPU/resident blur on an actual image using
  `--kernel global` or `--kernel shared`, with optional CSV and blurred PNG output.
- `blur_correctness`: known border/rounding cases and 120 generated GPU/CPU
  comparisons across both kernels, including tiny images and partial 16x16 tiles.
- `blur_benchmark_correctness`: validates sample counts, options and timed outputs.
- `image_correctness`: known colors, empty/invalid input, and 28 image comparisons
  across seven sizes and four patterns (black, white, alternating, seeded random).
  Includes 1, 255, 256, and 257 pixels and non-square images to test block boundaries.
  Also checks GPU/CPU agreement on four decoded PNG/JPEG/BMP fixtures.
- `image_io_correctness`: checks decoded pixels, row layout, alpha handling,
  lossless grayscale PNG roundtrips, file paths, and input/output error cases.
- `benchmark_correctness`: checks statistics, option bounds, measured sample
  counts, and correctness of timed GPU execution, including partial CUDA blocks.

CUDA allocation, copies, launch, and synchronization are checked. Missing GPUs and
CUDA errors produce a nonzero exit status; there is no CPU execution fallback.

## Load an image and save the grayscale result

After building, run from the repository root in PowerShell:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
.\build\image_demo.exe "C:\path\to\photo.jpg" "build\photo-gray.png"
.\build\image_demo.exe --help
```

Replace the input path with your own PNG, JPEG, or BMP file. The output must end
in `.png`, and its parent directory must already exist. Quote paths containing
spaces. Windows Unicode paths are supported. The app refuses to overwrite an
existing output, including the input itself; use a new output filename each time.

The app decodes packed RGB8 on the CPU, sends it to the CUDA kernel, verifies every
GPU output byte against the CPU reference, then writes an 8-bit single-channel
PNG at the original width and height. Open the PNG in any image viewer to see it.

PNG transparency is discarded without compositing; the underlying RGB colors are
processed. Grayscale input is expanded to RGB before conversion, and higher bit
depth is reduced to 8-bit. EXIF orientation and color-profile transformations are
not applied. Inputs are limited to 256 MiB encoded, 16384 pixels per dimension,
and `64 * 1024 * 1024` pixels total. Missing, malformed, or truncated images,
invalid dimensions, and output-write errors return a nonzero exit status.

The original generated-image demo remains available:

```powershell
.\build\image_demo.exe
```

The codecs are pinned to an exact upstream stb commit, with the upstream license
and SHA256 hashes in [third_party/stb/README.md](third_party/stb/README.md).

When file support was added, its three CTest checks passed on this machine, and
`C:\Windows\Web\Wallpaper\ThemeA\img20.jpg` was loaded as a 3840x2400 JPEG and
converted on the RTX 4060. All 9,216,000 pixels matched the CPU reference; the
viewable result was saved as `build\wallpaper-grayscale.png`. Generated outputs
remain ignored by Git; the Windows wallpaper is not included in the repository.

## Benchmark CPU versus GPU on an actual image

Build in **Release** mode using the commands below, then run from the repository
root (substitute your own PNG/JPEG/BMP path):

```powershell
. .\scripts\Enter-DevEnvironment.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=89
cmake --build build --parallel
.\build\image_benchmark.exe "C:\path\to\photo.jpg" --iterations 50 --warmup 5 --threads 256
```

To retain raw samples and a viewable result, use new output filenames:

```powershell
.\build\image_benchmark.exe "C:\path\to\photo.jpg" --iterations 50 --warmup 5 --csv "build\my-benchmark.csv" --output "build\my-benchmark-gray.png"
.\build\image_benchmark.exe --help
ctest --test-dir build --output-on-failure --verbose
```

Defaults are 50 measured iterations, 5 warmups, and 256 threads per block. Accepted
ranges are 1-10000 iterations and 1-1000 warmups. `--threads` accepts 1-1024 and
also checks the active GPU's limits and the compiled kernel's maximum. CSV and PNG outputs are optional, preserve existing
files, and require existing parent directories. Run without output options to
repeat benchmarks without creating files.

| Measurement | Timer and measured work |
| --- | --- |
| CPU full function | `steady_clock`; existing single-thread CPU conversion, including output allocation, with RGB already decoded in host memory |
| GPU full function | `steady_clock`; GPU conversion, including launch-limit validation, host output allocation, device allocations, RGB upload, kernel, synchronization, grayscale download, and device-buffer frees |
| GPU resident kernel | CUDA events around each kernel; device buffers are allocated and RGB uploaded once before timing, and final output is downloaded and checked afterward |

Both full-function results end in host memory. Returned output-vector destruction
is outside their timers. Host buffers use the current ordinary `std::vector`
allocations. Warmups and correctness comparisons are excluded from measurements.
CPU and full-GPU sample order alternates each iteration; their results are checked
against the CPU reference after every measurement. The resident-kernel phase has
its own warmups and validates its final output after all timed launches.

The same decoded input is reused, so this is a warm, repeated-image benchmark.
The CPU baseline uses one thread with compiler optimization enabled in Release.
GPU events are synchronized before reading elapsed time, following
[NVIDIA's timing guidance](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html#timing).
Kernel-only timing excludes transfers and allocation and represents a different
scope from a complete CPU-to-GPU-to-CPU conversion.

The report prints median, nearest-rank p95, minimum, and maximum in milliseconds.
Ratios divide the CPU median by the GPU median: above 1 favors GPU; below 1 favors
CPU. A zero GPU median reports `n/a` instead of dividing by zero. No outliers are
discarded. CSV contains one row per sample per method, with input path, dimensions,
warmup count, threads per block, calculated block count, build configuration,
method, iteration, and milliseconds. CPU rows carry the run's GPU configuration;
the CPU conversion itself still uses one thread. The older 2026-10-25 snapshot
predates these extra launch-configuration columns and used the fixed size 256.

Image decode and optional PNG encode/write are timed once and printed separately.
They are excluded from conversion speedups, as are CSV writing and console output.
For small images, GPU launch and transfer overhead can dominate. These measurements
describe the current implementations on the machine running the benchmark.

### Recorded result on this machine

On 2026-10-25, the Release build processed the existing 3840x2400 wallpaper JPEG
(9,216,000 pixels) on an AMD Ryzen 7 7800X3D and NVIDIA RTX 4060, driver 621.01.
Five warmups and 50 measured samples per method produced:

| Method | Median ms | P95 ms | Maximum ms |
| --- | ---: | ---: | ---: |
| CPU full function, one thread | 10.8746 | 11.9663 | 12.6549 |
| GPU full function, including transfers/allocations | 6.1857 | 7.7031 | 10.4124 |
| GPU resident kernel | 0.1605 | 0.1780 | 18.3027 |

The full GPU conversion was **1.7580x faster** by median time. The CPU/kernel-only
ratio was 67.7495x; it is not the speedup of a full conversion. The 18.3027 ms
kernel outlier is retained in the raw data. Decode took 48.8403 ms and PNG saving
took 188.3394 ms in this run; neither is included in those ratios.

All measured full-function outputs and the final resident-kernel output matched
the CPU reference. All six CTest checks passed. Statistics were also independently
recomputed from the CSV. See [benchmarks/README.md](benchmarks/README.md) for the
exact recorded command, environment, and all 150 raw samples. Timings vary with
image size, cache state, clocks, other applications, and GPU scheduling; tests
assert correctness and bookkeeping, never performance thresholds.

### Changing the block size

Compare sizes on the same image without rebuilding:

```powershell
foreach ($threads in 128, 256, 512) {
    .\build\image_benchmark.exe "C:\path\to\photo.jpg" --threads $threads --iterations 50 --warmup 5 --csv "build\threads-$threads.csv"
    if ($LASTEXITCODE -ne 0) { throw "Benchmark failed for $threads threads" }
}
```

Use new CSV names for subsequent comparisons. The console prints both the chosen
threads per block and the calculated number of blocks. The normal `image_demo`
continues to use the default of 256. Legal sizes need not be multiples of 32,
although warp-sized multiples are the usual starting point for tuning.

The setting travels through the code in four small steps:

1. `BenchmarkOptions::threads_per_block` in `include/benchmark.h` stores the value,
   with a default of 256.
2. `src/benchmark_main.cpp` parses `--threads`, prints the launch configuration,
   and records it in CSV. The code rejects zero, negative, malformed, and
   oversized values, including a missing option argument.
3. `src/benchmark.cpp` passes the setting to every full GPU warmup and measured
   conversion. The resident-kernel measurement reads the same options.
4. `src/grayscale.cu` uses that value in `<<<blocks, threads>>>`. The pixel-index
   calculation already uses `blockDim.x`, so each thread still finds the right
   pixel automatically. The bounds check handles partial final blocks.

The launch count remains ceiling division:

```cpp
const unsigned threads = threads_per_block;
const auto blocks = static_cast<unsigned>((gray.size() + threads - 1) / threads);
rgb_to_gray<<<blocks, threads>>>(input.data, output.data, gray.size());
```

`max_grayscale_threads_per_block()` queries the active device and the compiled
kernel. The minimum of the device's maximum block size, maximum x dimension, and
kernel's maximum block size is the supported limit. This machine reports 1024.
Those validation queries are included in the full-function timer and occur
outside the resident-kernel event window.

On 2026-10-26, three rounds of the 3840x2400 JPEG comparison produced these pooled
medians (150 samples per method and block size):

| Threads/block | Blocks | CPU ms | Full GPU ms | Kernel-only ms |
| ---: | ---: | ---: | ---: | ---: |
| 128 | 72,000 | 11.2667 | 6.1064 | 0.1585 |
| 256 | 36,000 | 11.3265 | 6.0764 | 0.1604 |
| 512 | 18,000 | 11.1190 | 6.1535 | 0.1628 |

128 had the lowest kernel median; 256 had the lowest full-conversion median.
The full-conversion medians differ by less than 1.3%, and per-round variation is
larger than that spread. These results do not establish a universal fastest
configuration. The default remains 256. All nine tests passed, and all 1350 raw
samples are retained in [the block-size comparison](benchmarks/block-sizes-2026-10-26.md).

## CPU, global-memory GPU, and shared-memory box blur

The 3x3 blur operates on grayscale8. Each output is the nearest-integer mean
`(sum + 4) / 9` of nine samples. Outside-image coordinates clamp to the nearest
border pixel; output dimensions are unchanged. Input and output buffers are
separate. Both GPU variants use fixed 16x16 blocks and 2D ceiling-division grids.

The global kernel reads nine global samples per output. The shared kernel loads
an 18x18 tile (including the one-pixel halo), synchronizes the whole block once,
then reads its nine samples from shared memory. Threads outside a partial image
tile still load and reach `__syncthreads()` before returning. The global kernel
needs no inter-thread barrier. Shared memory is an experiment, not a promise of
better performance: hardware caches can already reuse the basic kernel's reads.

Build with the existing Release commands, then compare an actual image:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
cmake --build build --parallel
ctest --test-dir build --output-on-failure
.\build\blur_benchmark.exe "C:\path\to\photo.jpg" --kernel global --iterations 50 --warmup 5 --csv "build\blur-global.csv" --output "build\blur-global.png"
.\build\blur_benchmark.exe "C:\path\to\photo.jpg" --kernel shared --iterations 50 --warmup 5 --csv "build\blur-shared.csv" --output "build\blur-shared.png"
.\build\blur_benchmark.exe --help
```

Choose new output filenames on each run. CSV and PNG output are optional and
preserve existing files. Kernel defaults to `global`; iterations/warmups default
to 50/5 with the same 1-10000 / 1-1000 ranges as grayscale. Blur does not expose
`--threads`; both variants use the same 16x16 shape to compare memory strategies.

Decode and CPU grayscale preparation happen once before blur timing. CPU/full GPU
use host timers and alternate order; full GPU includes host output/device
allocation, upload, launch, synchronization, download, and device frees. Resident
CUDA events bracket only kernels, with one upload before warmups and one final
download afterward. Every measured full result and final resident output are
checked against CPU. Warmups, validation, image I/O and CSV writing are excluded.
This is blur on already prepared grayscale, not an end-to-end RGB/blur/PNG timing.

CSV retains all CPU/full-GPU/kernel samples with image, kernel variant, warmups,
2D block/grid, build, method and iteration. The CPU baseline uses one thread;
its CSV rows carry GPU configuration metadata. Median/p95/min/max are shown;
no samples are discarded. Tests cover known edge/rounding cases, 120 generated
global/shared GPU comparisons, partial tiles, invalid input and benchmark counts.

On the RTX 4060, a four-round 3840x2400 comparison recorded pooled medians of
58.3966 ms CPU / 3.9728 ms full GPU / 0.1577 ms resident global kernel, and
58.3571 / 3.9458 / 0.1840 ms for shared. The global full blur was about 14.70x
faster than our single-thread CPU reference, with preparation/I/O excluded.
The shared kernel was slower; the small full-function difference changed
direction by round. See [all 1200 samples and methodology](benchmarks/blur-2026-10-26/README.md).

## Profiling build

Use a separate optimized build with CUDA source-line information:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
cmake -S . -B build-profile -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=89 -DIMAGE_CUDA_LINEINFO=ON
cmake --build build-profile --parallel
ctest --test-dir build-profile --output-on-failure
```

`IMAGE_CUDA_LINEINFO` adds `-lineinfo` only to CUDA compilation. This lets NVIDIA
tools connect GPU instructions to source lines while retaining Release
optimization; it does not enable the `-G` device-debug build mode. Keep timings
collected under a profiler separate from ordinary benchmark results.

On this Windows setup, add the matching NVIDIA tools with the selective toolkit
installer (the existing compiler/runtime packages remain installed):

```powershell
winget install --id Nvidia.CUDA --exact --version 13.4 --source winget --force --silent --accept-package-agreements --accept-source-agreements --disable-interactivity --override '-s nsight_compute_13.4 nsight_systems_13.4 sanitizer_13.4 -n'
.\scripts\Set-UserEnvironment.ps1
. .\scripts\Enter-DevEnvironment.ps1
nsys --version
ncu --version
compute-sanitizer --version
```

WinGet verifies the installer hash. Approve Windows' administrator prompt when
requested; no driver package is selected and `-n` prevents an automatic reboot.
`Set-UserEnvironment.ps1` backs up existing user settings and adds installed tool
directories to PATH. The developer-shell script also discovers these tools for
the current terminal.

After building, run each diagnostic separately:

```powershell
.\scripts\Profile-Grayscale.ps1 -Tool Systems -Image "C:\path\to\photo.jpg"
.\scripts\Profile-Grayscale.ps1 -Tool Compute -Image "C:\path\to\photo.jpg"
.\scripts\Profile-Grayscale.ps1 -Tool Memcheck -Image "C:\path\to\photo.jpg"
```

Each run creates a new folder under `build-profile\profiles`, prints its path,
and retains logs, diagnostic-only `instrumented-samples.csv`, the CMake cache, image/executable SHA256 hashes, Git revision
and working-tree status, GPU/driver identification, tool version, exact argument
arrays, and command exit codes in `metadata.json`. Optional `-OutputDirectory`
must name a new directory; existing reports are preserved. `-Threads` defaults
to 256; `-BuildDirectory` defaults to `build-profile` beside the source tree.

- **Systems** saves a `.nsys-rep` timeline and CSV summaries of CUDA API calls,
  kernels, GPU transfers, and individual GPU operations. CPU sampling and context
  switch collection are disabled; this capture focuses on CUDA activity.
- **Compute** saves a `.ncu-rep` or compressed `.ncu-repz` for the first full-conversion kernel after the
  three warmup launches. It collects launch configuration, occupancy, speed-of-light,
  and memory-workload sections, with kernel replay and cache flushing enabled.
  It leaves GPU clocks unchanged and imports source for line correlation. The
  saved report is also exported to `compute-details.log` and `compute-raw.csv`.
- **Memcheck** runs the existing image correctness tests, benchmark correctness
  tests, and actual-image benchmark under Compute Sanitizer, with full leak
  checking. `--error-exitcode 42` turns detected memory errors into a failed run.

The diagnostic benchmark uses 3 warmups and 10 iterations per method. The
complete run normally launches 26 kernels: 3 full-function warmups, 10 measured
full conversions, 3 resident-kernel warmups, and 10 measured resident kernels.
Compute collects one selected launch and may replay it several times. Printed
benchmark timings during any diagnostic include instrumentation effects and
must not be compared with ordinary benchmark results. Use the normal benchmark
commands above for performance measurements.

If Compute reports `ERR_NVGPUCTRPERM`, run the same command from an Administrator
PowerShell session; it needs permission to read GPU performance counters. There
is no need to change driver versions or globally open counter access.

If Memcheck reports `Failed to initialize WDDM debugger interface`, it has **not**
checked the kernel successfully, even if the application prints a correctness
pass. NVIDIA supplies `compute-sanitizer\EnableDebuggerInterface.bat` under
`CUDA_PATH`; it sets the machine registry value
`HKLM\SOFTWARE\NVIDIA Corporation\GPUDebugger\EnableInterface` (DWORD) to `1`.
Enabling that interface requires administrator rights and an explicit decision
to change this machine setting. The profiling script never changes it
automatically. Keep the failed log, then rerun Memcheck into a new output folder
after the interface is enabled. No successful sanitizer result should be claimed
from an initialization failure.

### Profile and check the blur

The existing script also accepts `-Operation BlurGlobal` and `BlurShared`;
omitting it preserves the original `Grayscale` behavior. Rebuild `build-profile`
after adding blur, using the profiling-build commands above, then run:

```powershell
.\scripts\Profile-Grayscale.ps1 -Tool Systems -Operation BlurGlobal -Image "C:\path\to\photo.jpg"
.\scripts\Profile-Grayscale.ps1 -Tool Compute -Operation BlurShared -Image "C:\path\to\photo.jpg"
.\scripts\Profile-Grayscale.ps1 -Tool Memcheck -Operation BlurShared -Image "C:\path\to\photo.jpg"
.\scripts\Profile-Grayscale.ps1 -Tool Racecheck -Operation BlurShared -Image ".\tests\fixtures\rgb.png"
.\scripts\Profile-Grayscale.ps1 -Tool Synccheck -Operation BlurShared -Image ".\tests\fixtures\rgb.png"
```

Compute still requires an Administrator PowerShell. Use either blur operation
with Systems/Compute/Memcheck to compare variants. Blur reports are named
`blur.nsys-rep` and `blur.ncu-rep`/`.ncu-repz`; the fixed 16x16 launch requires
the default `-Threads 256`.

Each sanitizer mode first runs the blur correctness and benchmark test executables
(which cover both kernels and partial tiles), then the selected actual-image
benchmark. Memcheck checks accesses/leaks; Racecheck checks shared-memory hazards;
Synccheck checks synchronization usage. Every mode uses error exit code 42.
Racecheck can be extremely slow on the 9.2-megapixel image; the small fixture
above is suitable for a quick walkthrough. Instrumented timing values have no
performance meaning. The actual-image runs and their logs are retained separately.

The first successful RTX 4060 timeline and retained CSV evidence are documented
in [the profiling record](benchmarks/profiling-2026-10-26/README.md). Its ten
measured full conversions showed median GPU activity durations of 2.420942 ms
for upload, 0.142385 ms for the kernel, and 0.928101 ms for download. These
diagnostic durations help explain the workload; they are not full-conversion
benchmark times.

After user-approved debugger enablement, Compute Sanitizer passed the image
correctness tests, benchmark tests, and actual-image benchmark with zero errors
and zero leaked bytes. An elevated Nsight Compute run captured and imported the
kernel report successfully. Its memory/launch/occupancy metrics and both the
initial failures and successful results are retained in the profiling record.

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

Initial setup checks (before file I/O was added):

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
Windows setup. Benchmark and profiling work added after this initial setup is
recorded in the sections above.
