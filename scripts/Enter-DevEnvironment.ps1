# Dot-source from PowerShell: . .\scripts\Enter-DevEnvironment.ps1
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Build Tools with C++ support is required.' }
$vsPath = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Visual Studio 2022 C++ tools were not found.' }
& (Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if (!$env:CUDA_PATH) { $env:CUDA_PATH = [Environment]::GetEnvironmentVariable('CUDA_PATH', 'Machine') }
if (!$env:CUDA_PATH) { $env:CUDA_PATH = [Environment]::GetEnvironmentVariable('CUDA_PATH', 'User') }
if (!(Test-Path -LiteralPath "$env:CUDA_PATH\bin\nvcc.exe")) { throw 'CUDA_PATH must point to an installed CUDA Toolkit.' }
$devPaths = @("$env:CUDA_PATH\bin", (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'), (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'))
$profilingTools = & (Join-Path $PSScriptRoot 'Get-ProfilingTools.ps1')
foreach ($profilingToolEntry in $profilingTools.PSObject.Properties) {
    if ($profilingToolEntry.Value) { $devPaths += Split-Path -Parent $profilingToolEntry.Value }
}
$devPaths = @($devPaths | Select-Object -Unique)
$env:PATH = ($devPaths -join ';') + ';' + $env:PATH
Write-Host "CUDA C++ environment ready: $env:CUDA_PATH"
