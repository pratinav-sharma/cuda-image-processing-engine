# Recorded real-image benchmark

The [first Nsight Systems timeline](profiling-2026-10-26/README.md) records CUDA
API, kernel, and transfer evidence. Profiling data is kept separate from normal
benchmark timing samples.

The later [block-size comparison](block-sizes-2026-10-26.md) tries 128, 256, and
512 threads per block over three rotated rounds, retaining all 1350 samples.
The original fixed-256 benchmark below is preserved unchanged.

`rtx4060-3840x2400.csv` is an unchanged copy of the 150 raw samples from a single
run on 2026-10-25. It includes every sample, including the slowest kernel event.
No filtering, best-run selection, or performance threshold was applied.

Command run from the repository root:

```powershell
. .\scripts\Enter-DevEnvironment.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=89
cmake --build build --parallel
.\build\image_benchmark.exe "C:\Windows\Web\Wallpaper\ThemeA\img20.jpg" --iterations 50 --warmup 5 --csv build\benchmark-wallpaper.csv --output build\benchmark-grayscale.png
```

Use new CSV/PNG paths to rerun; existing outputs are preserved. The source
wallpaper and generated grayscale PNG are not checked into Git. Benchmarking any
other supported image works without this machine's wallpaper file.

Environment and input:

- Native Windows 11 Home 26H2, build 10.0.26300, x64.
- CPU: AMD Ryzen 7 7800X3D; benchmark uses one CPU thread.
- GPU: NVIDIA GeForce RTX 4060, compute capability 8.9, WDDM driver 621.01.
- CUDA Toolkit 13.4.1, nvcc 13.4.59; runtime/driver APIs reported 13050.
- MSVC 19.44.35229; CMake 3.31.6-msvc6; Ninja 1.12.1; Release build.
- Input: JPEG, 3840x2400, 9,216,000 pixels.
- Input SHA256: `6610e4c12b5ccaa2906e7584339786378d921cbaf7c4b3bc5bd585e3f955b685`.
- Five excluded warmups and 50 measured samples for each of three methods.

The main README defines timing boundaries, statistics, output validation, and
interpretation. CPU and full GPU alternate order within each pair; the resident
kernel phase follows, using one upload and reusable device buffers. All measured
CPU/full-GPU outputs and the final resident-kernel output were checked against
the CPU reference outside the measured intervals.

The full GPU conversion's median speedup was 1.7580x. The kernel-only ratio of
67.7495x excludes host/device transfers and allocations. The maximum kernel time
was 18.3027 ms despite a 0.1605 ms median; both are preserved in this snapshot.
These are observations from this machine and image, not guaranteed speedups.
