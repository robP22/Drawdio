[CmdletBinding()]
param(
    [switch]$Reconfigure,
    [Alias('Jobs')]
    [ValidateRange(1, 256)]
    [int]$Parallel = 0,
    [string]$InstallDir,
    [switch]$NoInstall
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$rootDir = $PSScriptRoot
$buildDir = Join-Path $rootDir 'build'
$pluginName = 'Drawdio.vst3'

function Stop-Updater([string]$Message) {
    Write-Error $Message
    exit 1
}

function Invoke-CMake([string[]]$Arguments) {
    & cmake.exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        Stop-Updater "CMake failed with exit code $LASTEXITCODE"
    }
}

function Test-Vst3Bundle([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return $false
    }
    $contents = Join-Path $Path 'Contents'
    if (-not (Test-Path -LiteralPath $contents -PathType Container)) {
        return $false
    }
    return (
        (Test-Path -LiteralPath (Join-Path $contents 'Resources') -PathType Container) -or
        (Test-Path -LiteralPath (Join-Path $contents 'x86_64-win') -PathType Container)
    )
}

if (-not (Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
    Stop-Updater 'cmake.exe was not found on PATH'
}

$cachePath = Join-Path $buildDir 'CMakeCache.txt'
if ($Reconfigure -or -not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
    Write-Host 'Configuring Release build...'
    Invoke-CMake @('-S', $rootDir, '-B', $buildDir, '-DCMAKE_BUILD_TYPE=Release')
}

Write-Host 'Building Release VST3 + Standalone...'
$buildArguments = @('--build', $buildDir, '--config', 'Release', '--target', 'Drawdio_VST3', '--target', 'Drawdio_Standalone', '--parallel')
if ($Parallel -gt 0) {
    $buildArguments += $Parallel.ToString()
}
Invoke-CMake $buildArguments

$sourcePlugin = Join-Path $buildDir "Drawdio_artefacts\Release\VST3\$pluginName"
$standaloneDir = Join-Path $buildDir "Drawdio_artefacts\Release\Standalone"
$auBundle = Join-Path $buildDir "Drawdio_artefacts\Release\AU\Drawdio.component"
if (-not (Test-Vst3Bundle $sourcePlugin)) {
    Stop-Updater "Release VST3 bundle was not produced at $sourcePlugin"
}
if (-not (Test-Path -LiteralPath $standaloneDir -PathType Container)) {
    Stop-Updater "Release Standalone was not produced at $standaloneDir"
}

if ($NoInstall) {
    Write-Host "Validated: $sourcePlugin"
    Write-Host "Validated: $standaloneDir"
    exit 0
}

if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    $commonFiles = if (-not [string]::IsNullOrWhiteSpace($env:CommonProgramW6432)) {
        $env:CommonProgramW6432
    } elseif (-not [string]::IsNullOrWhiteSpace($env:CommonProgramFiles)) {
        $env:CommonProgramFiles
    } else {
        Join-Path $env:ProgramFiles 'Common Files'
    }
    $InstallDir = Join-Path $commonFiles 'VST3'
    $installDirExplicit = $false
} else {
    $installDirExplicit = $true
}

function Install-Vst3([string]$TargetDirectory, [string]$SourcePath) {
    $destination = Join-Path $TargetDirectory $pluginName
    $tempRoot = [System.IO.Path]::GetTempPath()
    $staging = Join-Path $tempRoot "Drawdio.vst3.staging.$PID"
    $stagedPlugin = Join-Path $staging $pluginName
    $backup = Join-Path $tempRoot "Drawdio.vst3.previous.$PID"
    $movedExisting = $false

    try {
        New-Item -ItemType Directory -Path $TargetDirectory -Force | Out-Null
        foreach ($legacy in @(Get-ChildItem -LiteralPath $TargetDirectory -Force -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '.Drawdio.vst3.*' })) {
            Remove-Item -LiteralPath $legacy.FullName -Recurse -Force -ErrorAction SilentlyContinue
        }
        if (Test-Path -LiteralPath $staging) {
            Remove-Item -LiteralPath $staging -Recurse -Force
        }
        if (Test-Path -LiteralPath $backup) {
            Remove-Item -LiteralPath $backup -Recurse -Force
        }
        if (Test-Path -LiteralPath $stagedPlugin) {
            Remove-Item -LiteralPath $stagedPlugin -Recurse -Force
        }
        New-Item -ItemType Directory -Path $staging -Force | Out-Null
        Copy-Item -LiteralPath $SourcePath -Destination $stagedPlugin -Recurse -Force

        if (-not (Test-Vst3Bundle $stagedPlugin)) {
            throw "staged VST3 bundle is incomplete"
        }

        if (Test-Path -LiteralPath $destination) {
            Move-Item -LiteralPath $destination -Destination $backup
            $movedExisting = $true
        }

        Move-Item -LiteralPath $stagedPlugin -Destination $destination
        Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $backup) {
            Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue
        }
        return $true
    } catch {
        if (Test-Path -LiteralPath $staging) {
            Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
        }
        if ($movedExisting -and -not (Test-Path -LiteralPath $destination) -and (Test-Path -LiteralPath $backup)) {
            Move-Item -LiteralPath $backup -Destination $destination -ErrorAction SilentlyContinue
        } elseif (Test-Path -LiteralPath $backup) {
            Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue
        }
        Write-Warning "Installation into '$TargetDirectory' failed: $($_.Exception.Message)"
        return $false
    }
}

Write-Host "Installing to $InstallDir..."
if (-not (Install-Vst3 $InstallDir $sourcePlugin)) {
    if (-not $installDirExplicit) {
        $perUserDir = Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
        if ($perUserDir -ne $InstallDir) {
            Write-Host 'System installation was not writable; trying per-user VST3 directory...'
            $InstallDir = $perUserDir
            if (Install-Vst3 $InstallDir $sourcePlugin) {
                Write-Host "Installed to $(Join-Path $InstallDir $pluginName)"
                Write-Host "Standalone: $standaloneDir"
                Write-Host 'Restart or rescan the DAW if it does not discover the updated plugin.'
                exit 0
            }
        }
    }
    Stop-Updater 'Could not install the VST3 bundle. Close any DAW using Drawdio, run PowerShell as Administrator, or pass -InstallDir to a writable directory.'
}

Write-Host "Installed to $(Join-Path $InstallDir $pluginName)"
Write-Host "Standalone: $standaloneDir"
Write-Host 'Restart or rescan the DAW if it does not discover the updated plugin.'
