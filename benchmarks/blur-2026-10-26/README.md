# Box blur comparison — 2026-10-26

CPU, global-memory GPU, and shared-memory GPU compute the same 3x3 grayscale8
filter: clamp neighbor coordinates to the edge, then round `(sum + 4) / 9`.
Clamping preserves constant-image brightness at the boundary and output size.
Both GPU variants use 16x16 blocks. The shared variant stages an 18x18 tile
(324 bytes), including a one-pixel halo, and synchronizes all threads before use.

## Reproduce the normal benchmark

Build and test from the repository root:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=89
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The recorded comparison used four rounds with alternating variant order, 5
warmups and 50 samples per method per run. Use a fresh directory:

```powershell
$runs = "build\my-new-blur-comparison"
if (Test-Path -LiteralPath $runs) { throw "Choose a new comparison directory" }
New-Item -ItemType Directory -Path $runs | Out-Null
for ($round = 1; $round -le 4; ++$round) {
    $order = if ($round % 2 -eq 1) { @("global", "shared") } else { @("shared", "global") }
    foreach ($kernel in $order) {
        .\build\blur_benchmark.exe "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" --kernel $kernel --iterations 50 --warmup 5 --csv "$runs\round-$round-$kernel.csv"
        if ($LASTEXITCODE -ne 0) { throw "Blur comparison failed" }
    }
}
```

Optional viewable output, using new filenames:

```powershell
.\build\blur_benchmark.exe "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" --kernel global --output "$runs\global-blur.png"
.\build\blur_benchmark.exe "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" --kernel shared --output "$runs\shared-blur.png"
```

For the recorded run, `--output` was included on each first-round invocation;
PNG saving follows conversion measurements. These local files are in
`build\blur-comparison-2026-10-26`. Both PNGs have SHA256
`b75096bc87efc1c8d14c7ba53d21c1c41e13c18b694719d9b76100c451752c98`.
Each output pixel had already matched the CPU reference before saving.

Environment and provenance:

- Source revision `3d5095c5ee3a222067330ece5bc960d2e0310e2c`, clean tree.
- Windows 11 Home x64; AMD Ryzen 7 7800X3D, single-thread CPU reference.
- NVIDIA GeForce RTX 4060, CC 8.9, WDDM driver 621.01.
- CUDA Toolkit 13.4.1 / nvcc 13.4.59; runtime/driver API 13050.
- MSVC 19.44.35229; CMake 3.31.6-msvc6; Ninja 1.12.1; Release, architecture 89.
- Input JPEG 3840x2400, 9,216,000 pixels; grid 240x150 blocks of 16x16 threads.
- Input SHA256 `6610e4c12b5ccaa2906e7584339786378d921cbaf7c4b3bc5bd585e3f955b685`.
- Executable SHA256 `097cc729e6644b9e4c7c7cb713c1daa963b0f845553a3a6a6e814277be68a9f4`.
- [Benchmark metadata](benchmark-metadata.json); the wallpaper and generated PNGs
  are local files rather than repository assets.

## Results and timing boundaries

[All 1200 raw samples](all-samples.csv) are retained: four rounds, two variants,
three methods and 50 samples. Each variant/method has 200 pooled samples. Median
uses the mean of the two central values; p95 is the 190th sorted sample.

| Variant | Method | Median ms | P95 ms | Min ms | Max ms |
| --- | --- | ---: | ---: | ---: | ---: |
| Global | CPU full | 58.396550 | 59.593700 | 57.532000 | 61.414400 |
| Global | GPU full | 3.972800 | 4.466600 | 3.684900 | 19.080100 |
| Global | GPU resident kernel | 0.157712 | 0.160768 | 0.156672 | 0.172032 |
| Shared | CPU full | 58.357100 | 59.928100 | 57.481600 | 61.358600 |
| Shared | GPU full | 3.945750 | 4.286400 | 3.667200 | 4.863800 |
| Shared | GPU resident kernel | 0.183968 | 0.200704 | 0.182272 | 0.208928 |

Full GPU blur was approximately **14.70x** (global) and **14.79x** (shared) faster
than the corresponding single-thread CPU reference by median. Full GPU includes
host output/device allocations, grayscale upload, kernel launch, wait, download,
and device frees. Returned host-vector destruction is outside both full timers.

RGB decoding and CPU grayscale preparation occur once **before** blur timers.
These speedups describe blur on prepared grayscale, not loading an RGB image,
converting it, blurring, and saving PNG end to end. Resident timing excludes
transfers and allocation and must not be presented as that pipeline's speedup.
Warmups, output comparisons, CSV and PNG writing are excluded from measurements.
CPU/full-GPU order alternates inside each run. The resident phase has its own
warmups, one upload, event-timed launches and one final validated download.

The shared kernel's median is **16.65% slower** than the global kernel here.
It is slower in every round's resident-kernel comparison:

| Round | Order | Global full ms | Shared full ms | Global kernel ms | Shared kernel ms |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | global, shared | 3.951650 | 3.893800 | 0.158336 | 0.183296 |
| 2 | shared, global | 3.966700 | 3.984800 | 0.158064 | 0.198320 |
| 3 | global, shared | 3.994400 | 3.921600 | 0.157696 | 0.183296 |
| 4 | shared, global | 3.988900 | 4.007550 | 0.158000 | 0.183296 |

Pooled full-GPU medians differ by only **0.68%**, and which variant is faster
changes by round. This is not a reliable full-conversion win for shared memory.
The default remains global. No samples, including the 19.0801 ms global outlier
and the slower shared second round, were removed.

This is one GPU, input image, filter size, block shape and CPU implementation.
Repeated warm-image caching, GPU clocks, Windows scheduling and other applications
affect results. The CPU reference is not a tuned SIMD or multithreaded library.
Other filters/shapes/hardware need new measurements. All 14 CTest checks passed
in both normal and profiling Release builds. No test asserts a speed threshold.

## GPU memory, synchronization and profiler evidence

The profiling Release build adds CUDA source lines while retaining optimization:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
cmake -S . -B build-profile -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=89 -DIMAGE_CUDA_LINEINFO=ON
cmake --build build-profile --parallel
ctest --test-dir build-profile --output-on-failure
$photo = "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg"
foreach ($operation in "BlurGlobal", "BlurShared") {
    .\scripts\Profile-Grayscale.ps1 -Tool Memcheck -Operation $operation -Image $photo
    .\scripts\Profile-Grayscale.ps1 -Tool Systems -Operation $operation -Image $photo
}
.\scripts\Profile-Grayscale.ps1 -Tool Racecheck -Operation BlurShared -Image $photo
.\scripts\Profile-Grayscale.ps1 -Tool Synccheck -Operation BlurShared -Image $photo
```

For counters, run these from an Administrator PowerShell in the repository:

```powershell
.\scripts\Profile-Grayscale.ps1 -Tool Compute -Operation BlurGlobal -Image "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg"
.\scripts\Profile-Grayscale.ps1 -Tool Compute -Operation BlurShared -Image "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg"
```

The wrapper was being extended during these runs: metadata records C++ source
revision `3d5095c` and the modified profiling script. The tested wrapper was then
published in `0c24f60`; kernel sources did not change between the captures and
that commit. The profiled executable SHA256 is
`8bfea2a46ec5fb73945ccf1c5da035bf4328a6507d96058ebcb5d770a5d9c8eb`.
Tools: Nsight Systems 2026.3.2.313, Nsight Compute 2026.3.0.0, Compute Sanitizer
2026.3.0.0. The previously approved NVIDIA debugger-interface setting remained
enabled; driver 621.01 was retained and no reboot was performed.

| Diagnostic | Result and retained evidence |
| --- | --- |
| Global Memcheck | Three commands passed; zero errors/leaks. [Metadata](Memcheck-BlurGlobal/metadata.json), [correctness](Memcheck-BlurGlobal/correctness.log), [benchmark tests](Memcheck-BlurGlobal/benchmark-tests.log), [real image](Memcheck-BlurGlobal/real-image.log) |
| Shared Memcheck | Three commands passed; zero errors/leaks. [Metadata](Memcheck-BlurShared/metadata.json), [correctness](Memcheck-BlurShared/correctness.log), [benchmark tests](Memcheck-BlurShared/benchmark-tests.log), [real image](Memcheck-BlurShared/real-image.log) |
| Shared Racecheck | Three commands passed; zero reported hazards/errors/warnings. [Metadata](Racecheck-BlurShared/metadata.json), [correctness](Racecheck-BlurShared/correctness.log), [benchmark tests](Racecheck-BlurShared/benchmark-tests.log), [real image](Racecheck-BlurShared/real-image.log) |
| Shared Synccheck | Three commands passed; zero reported errors. [Metadata](Synccheck-BlurShared/metadata.json), [correctness](Synccheck-BlurShared/correctness.log), [benchmark tests](Synccheck-BlurShared/benchmark-tests.log), [real image](Synccheck-BlurShared/real-image.log) |
| Global Systems | Capture/stats passed; 26 kernels, 14 uploads and 14 downloads. [Metadata](Systems-BlurGlobal/metadata.json), [GPU trace](Systems-BlurGlobal/summary_cuda_gpu_trace.csv), [CPU API summary](Systems-BlurGlobal/summary_cuda_api_sum.csv) |
| Shared Systems | Capture/stats passed; same counts. [Metadata](Systems-BlurShared/metadata.json), [GPU trace](Systems-BlurShared/summary_cuda_gpu_trace.csv), [CPU API summary](Systems-BlurShared/summary_cuda_api_sum.csv) |
| Global Compute | Capture/detail/raw export passed. [Metadata](Compute-BlurGlobal/metadata.json), [details](Compute-BlurGlobal/compute-details.log), [raw metrics](Compute-BlurGlobal/compute-raw.csv) |
| Shared Compute | Capture/detail/raw export passed. [Metadata](Compute-BlurShared/metadata.json), [details](Compute-BlurShared/compute-details.log), [raw metrics](Compute-BlurShared/compute-raw.csv) |

All 22 commands across these eight diagnostics exited zero. Each diagnostic app
run also completed output validation and produced 30 diagnostic samples. Checker
correctness tests cover both variants, tiny inputs, borders and partial tiles.
These are observations on the executed paths, not proof for all possible inputs.
Racecheck took about 25.4 minutes for the recorded actual-image command sequence;
use `tests\fixtures\rgb.png` instead for a shorter walkthrough. Checker timing
output can be enormous and is never part of the normal benchmark data above.

### What the kernel counters tell us

Both Compute captures select one full-conversion kernel after three warmups,
with kernel replay, cache flushing, source import and unchanged GPU clocks.
The tool warns that clocks can vary. These instrumented single-launch durations
are not the normal benchmark medians.

| Metric | Global | Shared |
| --- | ---: | ---: |
| Duration under profiling, microseconds | 168.90 | 238.30 |
| Registers/thread | 30 | 38 |
| Application static shared bytes/block | 0 | 324 |
| Achieved occupancy | 80.20% | 88.29% |
| Compute throughput / peak | 71.42% | 60.74% |
| DRAM throughput / peak | 21.03% | 14.88% |
| L1/TEX hit rate | 75.50% | 16.15% |

The shared version has higher achieved occupancy yet is slower. Occupancy alone
is not a performance score. The global version already obtains substantial
L1/TEX cache reuse. The shared version introduces cooperative tile indexing,
staging and a block barrier, and uses more registers. These observations are
consistent with that overhead outweighing the benefit of fewer explicit global
loads for this small filter. They do not isolate the cost of each instruction
or prove the barrier alone caused the slowdown. Hit-rate denominators differ
between the two access patterns; a lower shared-kernel cache hit rate is not
independently evidence of a problem.

For a complete 16x16 tile the source has 9*256 logical global input reads in
the basic version versus 18*18 cooperative input loads in the shared version.
Those source-level counts are not physical DRAM transaction counts: caches and
memory coalescing matter. We did not test larger filters, alternative tile sizes,
or perform instruction/stall-counter ablations in this milestone. See
[NVIDIA's metric definitions](https://docs.nvidia.com/nsight-compute/ProfilingGuide/index.html).

Local interactive reports are under `build-profile\profiles\blur-2026-10-26`:

- `Systems-BlurGlobal\blur.nsys-rep` and `Systems-BlurShared\blur.nsys-rep`.
- `Compute-BlurGlobal\blur.ncu-repz`, SHA256
  `a42be357240ee075f256c8301051575dc7f9c72edf3a9a9f75f750f38c803d0f`.
- `Compute-BlurShared\blur.ncu-repz`, SHA256
  `377d6423ba93a0b9760a08da9cd05493e6107322fd92d60c66d761d9183f4a79`.

The binaries, CMake caches, full session logs and instrumented sample CSVs remain
in ignored build directories. Portable evidence above is retained in Git. No
required check for this blur milestone remains blocked. CNN work is a separate
future project; the next interview-prep step is explaining and reproducing this work.
