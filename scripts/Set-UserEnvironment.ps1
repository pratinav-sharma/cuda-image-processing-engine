# Persist tool discovery without freezing Visual Studio's INCLUDE/LIB variables.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Enter-DevEnvironment.ps1')
$backupDir = Join-Path $env:LOCALAPPDATA 'cuda-image-processing-engine'
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
$backupFile = Join-Path $backupDir ('environment-before-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.json')
$oldPath = [Environment]::GetEnvironmentVariable('Path', 'User')
@{
    UserPath = $oldPath
    UserCudaPath = [Environment]::GetEnvironmentVariable('CUDA_PATH', 'User')
    MachineCudaPath = [Environment]::GetEnvironmentVariable('CUDA_PATH', 'Machine')
} | ConvertTo-Json | Set-Content -LiteralPath $backupFile -Encoding UTF8
$machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
$newPath = $oldPath
foreach ($entry in $devPaths) {
    if (!(Test-Path -LiteralPath $entry)) { throw "Missing tool directory: $entry" }
    $knownPaths = @($machinePath -split ';') + @($newPath -split ';')
    if ($knownPaths -notcontains $entry) {
        $newPath = if ([string]::IsNullOrEmpty($newPath)) { $entry } else { $newPath.TrimEnd(';') + ';' + $entry }
    }
}
[Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
[Environment]::SetEnvironmentVariable('CUDA_PATH', $env:CUDA_PATH, 'User')
Write-Host "Saved user PATH and CUDA_PATH. Previous values: $backupFile"
Write-Host 'Open a fresh terminal, then dot-source scripts/Enter-DevEnvironment.ps1 for MSVC.'
