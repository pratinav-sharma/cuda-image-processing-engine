# Return installed NVIDIA profiling tools without changing the environment.
$ErrorActionPreference = 'Stop'
$nvidiaRoot = Join-Path $env:ProgramFiles 'NVIDIA Corporation'
$tools = [ordered]@{}
foreach ($toolDefinition in @(
    @{ Name = 'nsys'; Folder = 'Nsight Systems *'; Relative = @('target-windows-x64\nsys.exe', 'nsys.exe') },
    @{ Name = 'ncu'; Folder = 'Nsight Compute *'; Relative = @('ncu.exe', 'target\windows-desktop-win7-x64\ncu.exe') },
    @{ Name = 'compute-sanitizer'; Folder = ''; Relative = @('compute-sanitizer\compute-sanitizer.exe') }
)) {
    $command = Get-Command $toolDefinition.Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $path = if ($command -and [IO.Path]::GetExtension($command.Source) -eq '.exe') { $command.Source } else { $null }
    if (!$path) {
        $roots = if ($toolDefinition.Folder) {
            @(Get-ChildItem -LiteralPath $nvidiaRoot -Directory -Filter $toolDefinition.Folder -ErrorAction SilentlyContinue |
                Sort-Object { [version]($_.Name -replace '^Nsight (Systems|Compute) ', '') } -Descending |
                Select-Object -ExpandProperty FullName)
        } else { @($env:CUDA_PATH) }
        foreach ($root in $roots) {
            if (!$root) { continue }
            foreach ($relative in $toolDefinition.Relative) {
                $candidate = Join-Path $root $relative
                if (Test-Path -LiteralPath $candidate -PathType Leaf) { $path = $candidate; break }
            }
            if ($path) { break }
        }
    }
    if (!$path -and $command) { $path = $command.Source }
    $tools[$toolDefinition.Name] = $path
}
[pscustomobject]$tools
