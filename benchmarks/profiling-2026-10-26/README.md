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
| Nsight Compute non-elevated capture | Failed with `ERR_NVGPUCTRPERM`, exit 1; no kernel-counter report |
| Nsight Compute elevated retry | Windows elevation was canceled; capture did not run |
| Compute Sanitizer | Failed to initialize the WDDM debugger interface, exit 42; no successful memory check |

The failed [Compute log](compute-permission.log) and
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

That registry key was absent when inspected. Enabling it needs administrator
rights and user approval to change the GPU debugger configuration; it was left
unchanged during this capture. After approval and successful enablement, rerun:

```powershell
.\scripts\Profile-Grayscale.ps1 -Tool Memcheck -Image "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" -OutputDirectory "build-profile\profiles\memcheck-enabled-retry"
```

Driver version 621.01 was preserved and no reboot was performed. Counter metrics,
source-correlated counter analysis, and successful GPU memory-error checking
remain unverified until these prerequisites are resolved.
