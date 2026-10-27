# Trying 128, 256, and 512 threads

Recorded on 2026-10-26 using the same Ryzen 7 7800X3D, RTX 4060, WDDM driver
621.01, CUDA 13.4.59 compiler, and Release build described in benchmarks/README.md.
The active GPU and compiled kernel report a 1024-thread limit for this launch.

Input: `C:\Windows\Web\Wallpaper\ThemeA\img20.jpg`, 3840x2400, 9,216,000 pixels.
SHA256: `6610e4c12b5ccaa2906e7584339786378d921cbaf7c4b3bc5bd585e3f955b685`.
The image is not included in the repository.

Three rounds rotated the configuration order:

```text
Round 1: 128, 256, 512
Round 2: 256, 512, 128
Round 3: 512, 128, 256
```

Every invocation used 5 warmups and 50 measured samples per method. CPU and full
GPU calls alternate within each invocation, followed by the resident-kernel
phase. Changing the order reduces consistent first/last-run bias; it does not
remove normal scheduling and clock variation. All output checks passed.

To reproduce the nine commands from the repository root, use fresh CSV paths:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
$orders = @(@(128,256,512), @(256,512,128), @(512,128,256))
for ($round = 1; $round -le 3; $round++) {
    foreach ($threads in $orders[$round-1]) {
        .\build\image_benchmark.exe "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" --threads $threads --iterations 50 --warmup 5 --csv "build\round-$round-threads-$threads.csv"
        if ($LASTEXITCODE -ne 0) { throw "Benchmark failed" }
    }
}
```

The recorded console logs and individual CSVs remain under
`build/block-size-2026-10-26/` on this machine. The checked-in
`block-sizes-2026-10-26.csv` combines all 1350 samples, adding only a `round`
column. Values are retained as emitted; no outliers or rounds were dropped.
Rows are grouped by round/configuration in the combined file; the execution
order is the rotation above. Each configuration/method has 150 samples.

## Pooled medians

| Threads/block | Blocks | CPU ms | Full GPU ms | Resident kernel ms |
| ---: | ---: | ---: | ---: | ---: |
| 128 | 72,000 | 11.26665 | 6.10635 | 0.158528 |
| 256 | 36,000 | 11.32650 | 6.07635 | 0.160432 |
| 512 | 18,000 | 11.11895 | 6.15345 | 0.162816 |

## GPU spread, including outliers

P95 uses the nearest-rank definition over the 150 samples for that row.

| Threads/block | Full GPU p95 ms | Full GPU max ms | Kernel p95 ms | Kernel max ms |
| ---: | ---: | ---: | ---: | ---: |
| 128 | 6.7089 | 13.9621 | 0.161792 | 0.172800 |
| 256 | 6.6586 | 7.4304 | 0.178176 | 0.267264 |
| 512 | 6.5402 | 18.3025 | 0.165888 | 0.169984 |

Per-round kernel medians show the variation behind the pooled values:

| Round | 128 threads ms | 256 threads ms | 512 threads ms |
| ---: | ---: | ---: | ---: |
| 1 | 0.158384 | 0.159696 | 0.162816 |
| 2 | 0.158624 | 0.160768 | 0.162816 |
| 3 | 0.158704 | 0.172032 | 0.162816 |

128 had the lowest kernel median, while 256 had the lowest pooled full-GPU
median. The full-GPU median spread is below 1.3%, smaller than observed run
variation. We keep 256 as the default. This is evidence for this image and
environment, not a guarantee that a particular size is fastest elsewhere.

The kernel's pixel math is identical across configurations. Full-GPU timings
include transfers, allocations, synchronization, and launch-limit validation;
kernel-only timing excludes those operations. Loading/saving images and output
validation are outside conversion timings. All nine CTest checks passed,
including legal non-warp-multiple sizes, partial blocks, and invalid limits.
