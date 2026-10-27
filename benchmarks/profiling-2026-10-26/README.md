# First grayscale timeline — 2026-10-26

Nsight Systems captured real CUDA execution on the NVIDIA GeForce RTX 4060
(WDDM, driver 621.01). The profiled application completed its CPU/GPU output
comparisons and wrote all 30 diagnostic samples. The separate Release profiling
build passed all nine CTest checks before capture.

## Reproduce the capture

From the repository root in PowerShell:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
cmake -S . -B build-profile -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=89 -DIMAGE_CUDA_LINEINFO=ON
cmake --build build-profile --parallel
ctest --test-dir build-profile --output-on-failure
.\scripts\Profile-Grayscale.ps1 -Tool Systems -Image "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" -OutputDirectory "build-profile\profiles\my-new-timeline"
```

Use a new output directory each time. Other PNG/JPEG/BMP images work, but timings
will differ. The Windows wallpaper is not included in this repository.

Recorded environment:

- Source revision: `89c67f7be621aaeeada477699bba3fd095843602`, clean working tree.
- Nsight Systems: `2026.3.2.313-263238521929v0`.
- CUDA Toolkit 13.4.1 / nvcc 13.4.59, MSVC 19.44.35229.
- CMake 3.31.6-msvc6 / Ninja 1.12.1; Release, architecture 89, `-lineinfo`.
- JPEG: 3840x2400, 9,216,000 pixels; 256 threads/block, 36,000 blocks.
- Input SHA256: `6610e4c12b5ccaa2906e7584339786378d921cbaf7c4b3bc5bd585e3f955b685`.
- Executable SHA256: `cf8641670f40d9f812d6a05b11684c16b89369b91c87345470d222663a4ce8cd`.
- Three warmups and ten measured iterations per benchmark method. CPU sampling
  and context-switch collection disabled; CUDA tracing enabled.

Exact argument arrays and zero exit codes are retained in
[systems-metadata.json](systems-metadata.json). Nsight did not forward the
application's console output on this Windows run. The script also checks that
the application's validated CSV contains ten samples per method and that the
trace contains 26 grayscale kernel launches.

## What the trace shows

Each full conversion allocates two device buffers, uploads **27.648 MB** of RGB,
launches the grayscale kernel, waits for completion, downloads **9.216 MB** of
grayscale, and frees the buffers. Host-side CUDA calls and GPU operations occupy
different timeline rows; their durations can overlap and must not be added
together as independent costs.

The ten measured full conversions, excluding the three warmups, produced these
GPU operation medians in this trace:

| GPU activity | Median ms |
| --- | ---: |
| RGB upload | 2.420942 |
| Grayscale kernel | 0.142385 |
| Grayscale download | 0.928101 |

Across these ten conversions, uploads and downloads accounted for 33.514523 ms
of the 34.938466 ms summed GPU copy/kernel durations, approximately **95.9%**.
The trace supports prioritizing data movement when investigating full-conversion
cost. Changing the kernel's block size alone does not remove those transfers.
Keeping intermediate images on the GPU when adding another processing stage is
a useful future experiment; this capture does not establish its eventual speedup.

These are profiler observations, not replacement benchmark results. GPU operation
durations exclude host allocation, pageable-buffer staging, API overhead, and
some waiting. They must not be presented as the complete CPU-to-GPU-to-CPU time.
The measured upload-to-download span has a median of 3.544516 ms, which also
excludes allocations before upload and frees after download. The normal
uninstrumented benchmark remains the source for speed comparisons.

## Read the evidence

- [GPU operation trace](summary_cuda_gpu_trace.csv): all 54 operations in time
  order, with timestamps, durations, transfer sizes, memory types, and launch
  configuration. It records pageable host memory, 16 registers/thread, and no
  shared memory for this kernel.
- [Full conversions](full-conversions.csv): the first 39 operations grouped as
  13 upload/kernel/download triplets; the first three are labeled warmup and the
  remaining ten measured. `gpu_span_ms` runs from upload start through download
  completion. No samples were discarded.
- [CUDA API summary](summary_cuda_api_sum.csv): CPU-side allocation, launch,
  synchronization, copies, events, and cleanup calls. Its percentages describe
  traced API time, not total program time.
- [Kernel summary](summary_cuda_gpu_kern_sum.csv) and
  [transfer summary](summary_cuda_gpu_mem_time_sum.csv): whole-capture aggregates.
  They include warmups and the resident-kernel phase, so counts differ: 26 kernels
  but only 14 uploads and 14 downloads.

There are 13 full-conversion launches (3 warmup + 10 measured). The resident phase
then uploads once, performs another 13 launches (3 warmup + 10 measured), and
downloads once. It reuses its allocations throughout that phase. This explains
the counts without treating transfers as part of every resident launch.

The local interactive report is:
`build-profile\profiles\2026-10-26-systems-final\grayscale.nsys-rep`.
Open it with the installed Nsight Systems application to inspect the CPU thread,
CUDA API, GPU kernel, and transfer rows. Its SHA256 is
`f02bbc857907c6274ec4f5849e3b6caeaef99a721cf29f2cb8882018aa95c6a1`.
The binary report, SQLite export, diagnostic timing samples, and full CMake cache
remain in the ignored build directory; portable CSV evidence is retained here.

## Other diagnostic checks

| Check | Actual result |
| --- | --- |
| Profiling Release build and CTest | Passed, 9/9 tests |
| Nsight Systems actual-image capture | Passed; 26 kernels and 30 validated diagnostic samples |
| Nsight Compute elevated capture and imports | Passed; one kernel captured, readable/raw reports exported, three commands exited 0 |
| Compute Sanitizer | Passed correctness tests, benchmark tests, and real-image benchmark; zero errors/leaks, all three commands exited 0 |

The initial failed [Compute log](compute-permission.log) and
[Sanitizer log](memcheck-interface.log), with their respective
[Compute metadata](compute-failed-metadata.json) and
[Sanitizer metadata](memcheck-failed-metadata.json), are retained. The application
printed correct-output checks during the failed attempts; these do not make the
profiler or sanitizer checks successful. In particular, the sanitizer's zero-leak
line is not evidence of clean GPU memory accesses when initialization failed.

To retry Compute, open an Administrator PowerShell, enter the repository, and
run the existing script with a fresh directory:

```powershell
Set-Location "C:\Users\prati\OneDrive\Documents\ChatGPT\New project\cuda-image-processing-engine"
.\scripts\Profile-Grayscale.ps1 -Tool Compute -Image "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" -OutputDirectory "build-profile\profiles\compute-admin-retry"
```

The Sanitizer error requests NVIDIA's installed script:
`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4\compute-sanitizer\EnableDebuggerInterface.bat`.
Its complete command is:

```bat
REG ADD "HKLM\SOFTWARE\NVIDIA Corporation\GPUDebugger" /f /v EnableInterface /t REG_DWORD /d 1
```

That registry key was absent when inspected. The user subsequently approved
enabling it. NVIDIA's script set `EnableInterface=1` successfully with administrator
rights. The [previous state](debugger-before.json) was saved before the change;
a second backup is under `%LOCALAPPDATA%\cuda-image-processing-engine` as
`gpu-debugger-before-20261026-224306-588.json`. No reboot was needed. Rerun with:

```powershell
.\scripts\Profile-Grayscale.ps1 -Tool Memcheck -Image "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" -OutputDirectory "build-profile\profiles\memcheck-enabled-retry"
```

### Successful memory checks

The completed run is in
`build-profile\profiles\approved-20261026-224124-570\memcheck`.
[Passed metadata](memcheck-passed-metadata.json) records source revision `8a475ef`,
the executable hash, exact arguments and zero exit codes. Logs:

- [Correctness](memcheck-correctness.log): known colors, invalid/empty input,
  28 generated comparisons and four decoded images; zero errors and zero leaks.
- [Benchmark tests](memcheck-benchmark-tests.log): statistics, counts, launch
  sizes/limits and timed GPU output agreement; zero errors and zero leaks.
- [Actual image](memcheck-real-image.log): 3840x2400 JPEG, 3 warmups and 10 samples
  per method, validated against CPU; zero errors and zero leaks.

These checks cover the executed paths and inputs. They are not a proof for every
possible input, and their instrumented timings are not performance benchmarks.

### Successful kernel-counter capture

The first elevated capture succeeded, but our import assumed `.ncu-rep` while
Nsight wrote compressed `.ncu-repz`. Commit `Read compressed GPU reports` fixed
the import path selection. A fresh full workflow at revision `01334b1` succeeded,
including capture, detail export, raw export, and all 30 validated app samples.
Its [metadata](compute-passed-metadata.json), [details](compute-details.log), and
[raw metrics](compute-raw.csv) are retained. The local interactive report is
`build-profile\profiles\approved-20261026-224124-570\compute-verified\grayscale.ncu-repz`,
SHA256 `02f314d39645b7b57b7e9e9fb5c498e7886b690590a05308d98ee90a2e2687c4`.

| Metric | Recorded value |
| --- | ---: |
| Kernel duration under profiling | 135.20 microseconds |
| DRAM throughput / peak | 88.73% |
| Memory throughput | 233.97 GB/s |
| Compute throughput / peak | 36.93% |
| Registers/thread | 16 |
| Threads/block; blocks | 256; 36,000 |
| Application static/dynamic shared bytes/block | 0; 0 |
| Theoretical / achieved occupancy | 100% / 76.72% |

The high DRAM throughput relative to compute throughput suggests this kernel is
primarily constrained by GPU memory bandwidth in this capture. Occupancy is the
active-warps fraction of the hardware maximum; it is not a direct speed score,
and the tool's estimated local speedup is not a measured optimization result.
See [NVIDIA's metric definitions](https://docs.nvidia.com/nsight-compute/ProfilingGuide/index.html).

The capture selects the first measured full-conversion kernel after three warmup
launches. Kernel replay and cache flushing are enabled. GPU clocks are left
unchanged, which the tool flags as a source of variability. It is one launch,
not a timing distribution, and excludes host/device transfers. Do not compare
its duration directly to uninstrumented median benchmark times.

Source import was enabled; `ncu --import <report> --page source --print-source cuda`
successfully displayed the CUDA source. The exported source view stays beside
the local report as `compute-source.txt`. Instruction-level source counters were
not requested in this baseline capture. Driver version 621.01 was preserved.
