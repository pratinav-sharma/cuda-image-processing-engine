param(
    [Parameter(Mandatory = $true)][ValidateSet('Systems', 'Compute', 'Memcheck', 'Racecheck', 'Synccheck')][string]$Tool,
    [Parameter(Mandatory = $true)][string]$Image,
    [ValidateSet('Grayscale', 'BlurGlobal', 'BlurShared')][string]$Operation = 'Grayscale',
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build-profile'),
    [string]$OutputDirectory,
    [ValidateRange(1, 1024)][int]$Threads = 256
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Enter-DevEnvironment.ps1')
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$inputImage = (Resolve-Path -LiteralPath $Image).Path
$isBlur = $Operation -ne 'Grayscale'
if ($isBlur -and $Threads -ne 256) { throw 'Blur profiling uses fixed 16x16 blocks (256 threads).' }
if (!$isBlur -and $Tool -in @('Racecheck', 'Synccheck')) { throw 'Use race/synchronization checks with a blur operation.' }
$executable = Join-Path $build $(if ($isBlur) { 'blur_benchmark.exe' } else { 'image_benchmark.exe' })
$kernelFunction = @{ Grayscale = 'rgb_to_gray'; BlurGlobal = 'box_blur_global'; BlurShared = 'box_blur_shared' }[$Operation]
$reportBase = if ($isBlur) { 'blur' } else { 'grayscale' }
$cache = Join-Path $build 'CMakeCache.txt'
if (!(Test-Path -LiteralPath $executable)) { throw 'Build the benchmark executable first.' }
$cacheText = Get-Content -LiteralPath $cache -Raw
if ($cacheText -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$' -or
    $cacheText -notmatch '(?m)^IMAGE_CUDA_LINEINFO:BOOL=ON\r?$') {
    throw 'Use a Release build configured with -DIMAGE_CUDA_LINEINFO=ON.'
}
$tools = & (Join-Path $PSScriptRoot 'Get-ProfilingTools.ps1')
$toolName = @{ Systems = 'nsys'; Compute = 'ncu'; Memcheck = 'compute-sanitizer'; Racecheck = 'compute-sanitizer'; Synccheck = 'compute-sanitizer' }[$Tool]
$toolPath = $tools.$toolName
if (!$toolPath) { throw "Install $toolName first; see README profiling setup." }
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $build ('profiles\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $Operation + '-' + $Tool)
}
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw "Output directory already exists: $output" }
New-Item -ItemType Directory -Path $output | Out-Null
$git = Get-Command git -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gitPath = if ($git) { $git.Source } else { Join-Path $env:ProgramFiles 'Git\cmd\git.exe' }
$metadata = [ordered]@{
    started = (Get-Date -Format o); status = 'running'; tool = $toolPath
    toolVersion = @(& $toolPath --version 2>&1 | ForEach-Object { "$_" })
    image = $inputImage; imageSHA256 = (Get-FileHash -LiteralPath $inputImage -Algorithm SHA256).Hash
    executable = $executable; executableSHA256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
    build = $build; threads = $Threads; operation = $Operation
    head = (& $gitPath -C $repo rev-parse HEAD)
    workingTree = @(& $gitPath -C $repo status --short)
    gpu = @(nvidia-smi --query-gpu=name,driver_version --format=csv,noheader)
    commands = @()
    note = 'Instrumented diagnostic run. Do not use printed benchmark timings as normal performance results.'
}
Copy-Item -LiteralPath $cache -Destination (Join-Path $output 'CMakeCache.txt')
function Invoke-Logged([string]$Program, [string[]]$Arguments, [string]$LogName) {
    $command = [ordered]@{ program = $Program; arguments = $Arguments; exitCode = $null }
    $metadata.commands += $command
    Write-Host ("Running: " + $Program + ' ' + ($Arguments -join ' '))
    # Windows PowerShell wraps native stderr in ErrorRecords. Preserve the log
    # and use the process exit code rather than mistaking stderr for failure.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Program @Arguments 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath (Join-Path $output $LogName)
        $command.exitCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($command.exitCode -ne 0) { throw "$LogName failed with exit code $($command.exitCode). See $output" }
}
$samplePath = Join-Path $output 'instrumented-samples.csv'
$benchmarkArguments = @($inputImage, '--iterations', '10', '--warmup', '3', '--csv', $samplePath)
if ($isBlur) {
    $benchmarkArguments += @('--kernel', $(if ($Operation -eq 'BlurGlobal') { 'global' } else { 'shared' }))
} else { $benchmarkArguments += @('--threads', "$Threads") }
try {
    Write-Host 'Profiler/sanitizer timings include instrumentation overhead; use ordinary benchmarks for speed comparisons.'
    switch ($Tool) {
        'Systems' {
            Invoke-Logged $toolPath (@('profile', '--trace=cuda', '--sample=none', '--cpuctxsw=none', '--show-output=true',
                '--output', (Join-Path $output $reportBase), $executable) + $benchmarkArguments) 'systems.log'
            Invoke-Logged $toolPath @('stats', '--report', 'cuda_api_sum,cuda_gpu_kern_sum,cuda_gpu_mem_time_sum,cuda_gpu_trace',
                '--format', 'csv', '--output', (Join-Path $output 'summary'), (Join-Path $output "$reportBase.nsys-rep")) 'systems-stats.log'
            $trace = @(Import-Csv -LiteralPath (Join-Path $output 'summary_cuda_gpu_trace.csv'))
            if (@($trace | Where-Object Name -Like "*$kernelFunction*").Count -ne 26) {
                throw 'Expected 26 selected kernels in the Systems trace; inspect the saved reports.'
            }
        }
        'Compute' {
            # Skip the three full-function warmup launches and profile the first
            # measured full-function kernel. Let the application finish validation.
            Invoke-Logged $toolPath (@('--kernel-name', "regex:$kernelFunction", '--launch-skip', '3', '--launch-count', '1',
                '--section', 'SpeedOfLight', '--section', 'LaunchStats', '--section', 'Occupancy',
                '--section', 'MemoryWorkloadAnalysis', '--replay-mode', 'kernel', '--cache-control', 'all',
                '--clock-control', 'none', '--import-source', 'yes', '--export', (Join-Path $output $reportBase),
                $executable) + $benchmarkArguments) 'compute.log'
            $reports = @("$reportBase.ncu-rep", "$reportBase.ncu-repz" | ForEach-Object { Join-Path $output $_ } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
            if ($reports.Count -ne 1) { throw 'Expected exactly one Nsight Compute report after capture.' }
            $metadata.report = $reports[0]
            Invoke-Logged $toolPath @('--import', $reports[0], '--page', 'details') 'compute-details.log'
            Invoke-Logged $toolPath @('--import', $reports[0], '--page', 'raw', '--csv') 'compute-raw.csv'
        }
        { $_ -in @('Memcheck', 'Racecheck', 'Synccheck') } {
            $checks = @('--tool', $Tool.ToLowerInvariant(), '--error-exitcode', '42')
            if ($Tool -eq 'Memcheck') { $checks += @('--leak-check', 'full') }
            $correctness = if ($isBlur) { @((Join-Path $build 'blur_correctness.exe')) } else {
                @((Join-Path $build 'image_correctness.exe'), (Join-Path $repo 'tests\fixtures'))
            }
            $benchmarkTest = Join-Path $build $(if ($isBlur) { 'blur_benchmark_correctness.exe' } else { 'benchmark_correctness.exe' })
            Invoke-Logged $toolPath ($checks + $correctness) 'correctness.log'
            Invoke-Logged $toolPath ($checks + @($benchmarkTest)) 'benchmark-tests.log'
            Invoke-Logged $toolPath ($checks + @($executable) + $benchmarkArguments) 'real-image.log'
        }
    }
    # The application writes CSV only after every measured output has matched
    # the CPU reference. This also works when Windows hides its console output.
    $samples = @(Import-Csv -LiteralPath $samplePath)
    foreach ($method in 'cpu', 'gpu_full', 'gpu_kernel') {
        if (@($samples | Where-Object method -EQ $method).Count -ne 10) {
            throw "Missing validated samples for $method in the profiled application output."
        }
    }
    if ($samples.Count -ne 30) { throw 'Unexpected diagnostic sample count.' }
    $metadata.status = 'completed'
} catch {
    $metadata.status = 'failed'
    $metadata.error = $_.Exception.Message
    throw
} finally {
    $metadata.finished = Get-Date -Format o
    $metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'metadata.json') -Encoding UTF8
    Write-Host "Reports and logs: $output"
}
