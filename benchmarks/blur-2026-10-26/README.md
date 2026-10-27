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
