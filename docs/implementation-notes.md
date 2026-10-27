# Implementation journal and interview notebook

Read this alongside the code. For each change, practice explaining the decision
in your own words, then open the diff with `git show <hash>` and find the lines
that implement it. New entries belong in the same commit as their implementation.
They use the commit subject so they can be written before its hash exists.

The historical entries below were reconstructed from Git history and retained
setup/benchmark records. Validation refers to those recorded milestones, not a
fresh build of every historical revision. Current commands live in the main
[README](../README.md); measurements live in [benchmarks](../benchmarks/README.md).

## What we are building

The input is an RGB image in CPU memory. Grayscale produces one brightness byte
per pixel. Our next operation is a 3x3 box blur on that grayscale image: average
a pixel and its eight neighbors. We will compare CPU, basic GPU, and shared-memory
GPU implementations, keeping exactly the same edge and rounding rules.

The planned blur rule is **clamp to edge**: outside-image coordinates use the
nearest border pixel. Output dimensions stay unchanged. Integer rounding will
be `(sum + 4) / 9`. This is a fixed filter; a separately planned CNN will learn
weights from training data in another project.

## Existing commits

### 06a518a — Add Windows CUDA developer environment helpers

- **Problem/approach:** nvcc needs both CUDA and MSVC tools available. Initialize
  Microsoft's developer shell per session; persist tool discovery separately.
  Avoid permanently freezing compiler-specific INCLUDE/LIB paths.
- **Files:** `scripts/Enter-DevEnvironment.ps1`, `scripts/Set-UserEnvironment.ps1`.
- **Validation:** initial setup record documents compatible compiler detection,
  saved environment values, and subsequent native Windows builds.
- **Practice:** Why does finding nvcc alone not prove CUDA C++ can compile?

### dcdf4f3 — Implement deterministic CPU RGB-to-grayscale reference

- **Problem/approach:** establish an independently implemented expected answer.
  Packed RGB uses three bytes per pixel. Compute
  `(77*R + 150*G + 29*B + 128) / 256` with integer arithmetic for exact comparisons.
  The coefficients sum to 256; adding 128 provides nearest-integer rounding.
- **Files:** `include/grayscale.h`, `src/grayscale.cpp`.
- **Validation:** known colors and input validation were checked during setup.
- **Practice:** What does the header declare, and where is the implementation?

### 4cb0e44 — Add CUDA grayscale kernel and CMake GPU demo

- **Problem/approach:** give each thread one output pixel using
  `blockIdx.x * blockDim.x + threadIdx.x`. A bounds check protects the partial
  last block. Allocate separate device input/output, upload RGB, launch, wait,
  download grayscale, and free through RAII objects.
- **Files:** `src/grayscale.cu`, `src/main.cpp`, `CMakeLists.txt`.
- **Validation:** generated 1920x1080 image executed on the RTX 4060 and matched CPU.
- **Practice:** Why is there no `__syncthreads()` here? Each thread owns its output
  and reads immutable input; no thread consumes another thread's intermediate work.

### 95021ac — Test CUDA grayscale against CPU across image patterns and block boundaries

- **Problem/approach:** a good-looking output can conceal indexing errors. Check
  known colors plus black, white, alternating and seeded-random patterns across
  seven sizes, including 1, 255, 256 and 257 pixels and non-square images.
- **Files:** `tests/correctness.cpp`, CTest registration in `CMakeLists.txt`.
- **Validation:** the original correctness target passed all 28 generated cases.
- **Practice:** Why is 257 pixels more revealing than testing only 256?

### dcc2f84 — Document Windows CUDA setup commands and verified GPU results

- **Problem/approach:** make setup reproducible and distinguish installed toolkit,
  driver, and runtime versions. Record real hardware execution and exact commands.
- **Files:** `README.md`, ignore rules for generated build files.
- **Validation:** retained initial report includes nvidia-smi, successful build,
  demo CPU/GPU agreement, and CTest. Driver changes and reboot were avoided.
- **Practice:** Why can nvidia-smi's CUDA version differ from nvcc's version?

### 689ec64 — Vendor pinned stb image decoding and PNG encoding headers

- **Problem/approach:** read actual image files without a large framework.
  Vendor the two required headers at an exact upstream revision, retain license
  and hashes, and compile their implementations in our image I/O library.
- **Files:** `third_party/stb`, `src/image_io.cpp`, `include/image_io.h`.
- **Validation:** subsequent image I/O tests and actual JPEG conversion exercised them.
- **Practice:** Why pin a dependency instead of copying an unspecified latest file?

### 938234a — Load RGB images and save CUDA-verified grayscale PNG output

- **Problem/approach:** decode PNG/JPEG/BMP into top-to-bottom packed RGB8, run
  CUDA grayscale, compare every byte with CPU, then encode single-channel PNG.
  Preserve existing output files; support Windows paths through filesystem paths.
- **Files:** `src/image_io.cpp`, `src/main.cpp`, library wiring in CMake.
- **Validation:** the actual 3840x2400 wallpaper conversion matched all 9,216,000 pixels.
- **Practice:** Which steps run on CPU, and which step runs on GPU?

### 9ceed15 — Test image file decoding, grayscale PNG output, and CLI behavior

- **Problem/approach:** cover codec fixtures, BMP row padding/orientation, alpha
  handling, PNG roundtrips, Unicode/spaces, corrupt inputs, and overwrite refusal.
- **Files:** `tests/image_io.cpp`, `tests/fixtures`, CTest targets.
- **Validation:** the file-I/O milestone passed its three CTest checks.
- **Practice:** Why can a pixel kernel be correct while the image still looks wrong?

### 7013cf9 — Document image conversion usage, formats, and verified file results

- **Problem/approach:** document exact commands and restrictions. Alpha is discarded
  without compositing; EXIF orientation and color profiles are not transformed.
- **Files:** `README.md`.
- **Validation:** documented commands correspond to the retained actual-image result.
- **Practice:** What does our application do with a transparent PNG?

### 91af61d — Measure CPU conversion, complete GPU conversion, and CUDA kernel timings

- **Problem/approach:** separate useful timing scopes. Use steady_clock for full
  host-to-host functions, and CUDA events for a kernel whose buffers stay on GPU.
  Exclude warmups, output checks, loading and saving. Alternate CPU/full-GPU order.
- **Files:** `include/benchmark.h`, `src/benchmark.cpp`, `src/grayscale.cu`.
- **Validation:** later benchmark tests check sample counts and timed output correctness.
- **Practice:** Why doesn't a fast resident kernel prove an equally fast full conversion?

### acf8ddf — Add real-image benchmark command with timing summaries and raw CSV export

- **Problem/approach:** expose iteration/warmup counts, retain every timing sample,
  and summarize median, nearest-rank p95, minimum and maximum. Warmups are practice
  executions excluded from reported samples. Preserve existing CSV/PNG outputs.
- **Files:** `src/benchmark_main.cpp`.
- **Validation:** real-image benchmark and CLI tests exercised parsing and CSV output.
- **Practice:** Why retain raw samples instead of publishing just the best time?

### d43bb3d — Verify benchmark statistics, sample counts, and timed GPU correctness

- **Problem/approach:** validate statistics using small known sample lists; reject
  invalid options and verify GPU results for tiny and partial-block inputs.
  Test correctness/bookkeeping rather than brittle performance thresholds.
- **Files:** `tests/benchmark.cpp`, CMake tests.
- **Validation:** benchmark milestone passed six CTest checks.
- **Practice:** Why is a test like 'GPU must finish under 1 ms' unreliable here?

### 2264606 — Document benchmark methodology and retain all real-image timing samples

- **Problem/approach:** retain all 150 samples and explain scopes and limitations.
  Recorded CPU/full-GPU medians were 10.8746/6.1857 ms, about 1.758x speedup;
  the 0.1605 ms kernel median excludes transfers and allocation.
- **Files:** `benchmarks/rtx4060-3840x2400.csv`, `benchmarks/README.md`, main README.
- **Validation:** CSV statistics were independently recomputed; outliers were retained.
- **Practice:** What are the CPU baseline and repeated-image benchmark's limitations?

### 7fd6e97 — Let GPU launches pick a block size

- **Problem/approach:** replace a fixed launch constant with a defaulted parameter.
  Keep ceiling division, and query device plus compiled-kernel launch limits.
  The pixel calculation already uses blockDim.x, so indexing adapts automatically.
- **Files:** `include/grayscale.h`, `src/grayscale.cu`, benchmark options/callers.
- **Validation:** block-size correctness cases were added in the later test commit.
- **Practice:** Why is 256 a useful starting choice rather than a GPU-specific guarantee?

### 98f0b78 — Add a --threads option

- **Problem/approach:** carry CLI choice through options to both GPU timing scopes.
  Report threads and calculated blocks in console/CSV; reject malformed or illegal values.
- **Files:** `src/benchmark_main.cpp`, benchmark call sites.
- **Validation:** CLI smoke tests and the subsequent real-image comparison exercise it.
- **Practice:** How many blocks do 9,216,000 pixels need at 128, 256 and 512 threads?

### 1a5c3ac — Check a few block sizes

- **Problem/approach:** compare valid launch sizes on identical inputs, including
  1/33-thread cases and 257/513-pixel partial blocks. Check the device/kernel maximum.
- **Files:** `tests/benchmark.cpp`, CTest CLI checks for 128/256/512.
- **Validation:** all nine CTest checks passed.
- **Practice:** Do all legal block sizes need to be multiples of a 32-thread warp?

### ac3e84e — Save block size results

- **Problem/approach:** rotate 128/256/512 order across three rounds to reduce
  consistent order bias; keep all 1350 samples. 128 had the lowest pooled kernel
  median, 256 the lowest full-GPU median; full-GPU spread was under 1.3%.
- **Files:** `benchmarks/block-sizes-2026-10-26.md` and its CSV.
- **Validation:** outputs matched CPU; per-round and pooled data were retained.
- **Practice:** Why do these results not establish a universal fastest block size?

### dcb20fe — Explain the new threads option

- **Problem/approach:** document parameter flow, validation, launch-count formula
  and comparison results. Retain 256 as default because variation exceeds the
  small full-conversion differences.
- **Files:** `README.md`.
- **Validation:** documentation links the recorded three-round experiment.
- **Practice:** Explain ceiling division and the partial final block without a script.

### 655d14b — Add profiling build support

- **Problem/approach:** add opt-in CUDA `-lineinfo` so tools can connect GPU
  instructions to source lines while keeping Release optimization. Use a separate
  build directory; do not use device-debug `-G` for performance investigation.
- **Files:** `CMakeLists.txt`, `README.md`.
- **Validation:** inspected nvcc command and passed all nine profiling-build tests.
- **Practice:** What is the difference between line information and disabling optimization?

### 86413e3 — Find the profiling tools

- **Problem/approach:** discover installed NVIDIA executables, append directories
  to the developer shell, and reuse the existing backed-up persistent PATH setup.
- **Files:** `scripts/Get-ProfilingTools.ps1`, `scripts/Enter-DevEnvironment.ps1`.
- **Validation:** discovered installed versions and verified persistent tool directories.
- **Practice:** What does each of Systems, Compute and Compute Sanitizer tell us?

### 89c67f7 — Add repeatable GPU checks

- **Problem/approach:** one script chooses a diagnostic, refuses existing output
  folders, records commands/build/input hashes, and treats nonzero tool exits as
  failures. Diagnostic CSV validates the target completed its CPU/GPU comparisons.
- **Files:** `scripts/Profile-Grayscale.ps1`, README, tool discovery adjustment.
- **Validation:** Systems smoke capture, overwrite protection, invalid build rejection,
  and real permission/interface failures were checked. Memory errors use exit 42.
- **Practice:** Why doesn't the application's PASS line prove the profiler succeeded?

### 8a475ef — Save the first GPU timeline

- **Problem/approach:** retain portable CSV evidence, distinguish CPU API time
  from GPU activity, and label failed checks. Full measured conversion GPU medians:
  upload 2.420942 ms, kernel 0.142385 ms, download 0.928101 ms.
- **Files:** `benchmarks/profiling-2026-10-26`, README.
- **Validation:** 26 kernels, 14 uploads, 14 downloads, and 30 validated diagnostic samples.
  Initial Compute/Sanitizer failures were preserved rather than described as success.
- **Practice:** Why must overlapping CPU API and GPU durations not be added together?

## New commits and ongoing study notes

### Start our interview notebook

- **Problem/approach:** code history alone does not preserve our reasoning for
  interview preparation. Reconstruct the implementation sequence, link evidence,
  and require future implementation commits to include their own study notes.
- **Files:** this journal, `AGENTS.md`, README link.
- **Validation:** compared all 22 existing development commit subjects with Git
  history and checked file links. Historical revisions were not rebuilt individually.
- **Practice:** Trace one input image through decoding, CPU reference, GPU memory,
  kernel indexing, correctness checking, and output saving in your own words.

### Read compressed GPU reports

- **Problem/approach:** Nsight Compute 2026.3.0 successfully captured a kernel but
  wrote `.ncu-repz`; our import step assumed `.ncu-rep`. Discover exactly one of
  the two report formats, record its path, and import that file. Preserve the
  original report and failed postprocessing metadata.
- **Files:** `scripts/Profile-Grayscale.ps1`, README.
- **Validation:** manually imported the captured compressed report and read its
  launch, occupancy and memory sections. A fresh complete workflow is recorded
  with the following profiling evidence update.
- **Practice:** How can capture succeed even when the wrapper script reports failure?

### Save the GPU checks

- **Problem/approach:** resolve the initial permission/interface blockers with
  user-approved debugger enablement and an elevated capture. Save successful
  memory checks and kernel metrics, while preserving the original failures.
- **Files:** `benchmarks/profiling-2026-10-26`, README.
- **Validation:** all three Memcheck commands exited 0 with zero reported errors
  and leaked bytes. Fresh Compute capture/import/raw export all exited 0 and
  the app produced 30 validated samples. Source import also displayed CUDA code.
  Driver stayed 621.01; no reboot or global PowerShell policy change.
- **Finding:** 88.73% DRAM versus 36.93% compute throughput suggests GPU-memory
  bandwidth pressure. 76.72% achieved occupancy is not an automatic speedup claim.
- **Practice:** Explain what Memcheck proved for these runs, what it did not prove,
  and why profiler timings are separate from normal benchmark results.

### Add a CPU box blur

- **Problem/approach:** create the expected answer for a 3x3 filter on grayscale8.
  Clamp each neighbor to the image boundary, average all nine samples, and round
  with `(sum + 4) / 9`. Separate output preserves the original neighbor values.
- **Files:** `include/blur.h`, `src/blur.cpp`, `tests/blur.cpp`, CMake test target.
- **Validation:** hand-calculated 1x1, 1x3, 3x1, 2x2 and impulse cases, rounding
  around 4/9 versus 5/9, uniform white, and dimension/size rejection. The ten-check
  suite passed after adding this target.
- **Practice:** Why does clamping count a corner pixel more than once, and why
  must this blur not overwrite its input while calculating later outputs?

### Run the blur on the GPU

- **Problem/approach:** use a 2D grid of 16x16 blocks. Each thread computes one
  `(x,y)` output and reads nine clamped samples from immutable global input.
  Separate output avoids neighbors changing during the calculation. Partial
  blocks return before any memory access; this kernel has no inter-thread barrier.
- **Files:** `src/blur.cu`, `include/blur.h`, expanded `tests/blur.cpp`, CMake.
- **Validation:** known-value GPU results and 60 generated pattern/shape comparisons
  against CPU, including single rows/columns and 15/16/17 boundaries. The ten-check
  CTest suite passed on the RTX 4060.
- **Practice:** How does a 2D thread coordinate map to a row-major pixel address,
  and why don't these threads need to wait for one another?

### Share pixels within each block

- **Problem/approach:** neighboring 3x3 outputs reread overlapping pixels. Stage
  an 18x18 input tile (including its one-pixel halo) in shared memory for each
  16x16 block. Threads cooperatively load unique tile cells, synchronize once,
  then compute from the tile. All threads reach the barrier before bounds returns.
- **Files:** `src/blur.cu`, `include/blur.h`, both variants in `tests/blur.cpp`.
- **Validation:** hand-calculated cases and 120 generated CPU/GPU comparisons
  passed in the ten-check suite, including partial tiles and single-pixel images.
  Sanitizer/race/synchronization checks and timing comparisons are recorded later.
- **Practice:** What is a halo? Why is returning early safe in the global kernel
  but unsafe before the shared kernel's barrier? Why might caches make this
  shared version slower despite fewer explicit global reads?

### Time both blur kernels

- **Problem/approach:** measure CPU and complete GPU blur using a host timer, and
  resident kernel execution using CUDA events. Reuse one uploaded grayscale
  buffer for resident launches. Keep decoding and grayscale preparation outside
  blur timers; alternate CPU/full-GPU order and validate every measured output.
- **Files:** `src/blur_benchmark.cpp`, `src/blur.cu`, `include/blur_benchmark.h`,
  `tests/blur_benchmark.cpp`. Both blur kernels use fixed 16x16 blocks.
- **Validation:** counts/output checked for global/shared on 1x19 and 17x19 inputs;
  unsupported block settings and invalid iteration counts rejected. Eleven CTest
  checks passed; no performance thresholds were asserted.
- **Practice:** Why must CUDA events be synchronized before reading elapsed time,
  and why does resident timing need a separate full-function measurement?

### Try the blur on a real image

- **Problem/approach:** expose `blur_benchmark` with global/shared selection,
  optional verified PNG output, and every raw timing sample in CSV. Prepare
  grayscale on CPU before the timers so the comparison isolates the blur.
- **Files:** `src/blur_benchmark_main.cpp`, CMake CLI tests, README commands.
- **Validation:** both fixture CLI variants, help, and the full 14-check suite
  passed. Actual-image output and repeated performance results are saved in the
  following evidence checkpoint; printed blur speedup excludes preparation/I/O.
- **Practice:** Which work is excluded from the full blur timer, and why is that
  different from the complete input-image-to-output-file pipeline?

### Profile and check the blur

- **Problem/approach:** reuse the profiling workflow with an operation selector
  rather than duplicating it. Choose the correct executable/kernel filter and
  test suite, preserve grayscale defaults, and add Racecheck/Synccheck for shared
  memory and barrier validation. Each diagnostic retains its own metadata/logs.
- **Files:** `scripts/Profile-Grayscale.ps1`, README usage and target list.
- **Validation:** both Memcheck variants, shared Racecheck/Synccheck, both Systems
  traces, and the original grayscale Systems smoke test completed successfully.
  All 14 profiling-build tests passed. The counter reports are retained in the
  subsequent comparison evidence update.
- **Practice:** Which bugs would Racecheck/Synccheck find that a CPU/GPU output
  comparison might miss? Why is a sanitizer's very slow timing not a GPU speed result?

### Save the blur comparison

- **Problem/approach:** compare both variants across four balanced-order rounds
  and retain all 1200 samples. Report full-function and resident scopes separately;
  preserve outliers and the result even when shared memory does not win.
- **Files:** `benchmarks/blur-2026-10-26/README.md`, raw CSV and benchmark metadata;
  main README result summary.
- **Validation:** 200 samples per variant/method, recomputed medians/p95, all
  measured outputs checked against CPU, identical saved PNG hashes, overwrite and
  invalid-extension rejection. Global full blur was 14.70x faster than the current
  single-thread CPU reference; the shared kernel was 16.65% slower than global.
- **Practice:** Explain why this is a useful optimization experiment even though
  shared memory lost, and qualify the 14.70x number without implying full PNG-pipeline speedup.

## Entry template

### Short commit subject

- **Problem/approach:** what changed, why, and the tradeoff we chose.
- **Files:** the important implementation points to open while studying.
- **Validation:** exact tests/commands run, outcome, and anything still blocked.
- **Practice:** one question to answer without reading a prepared script.
