<#
.SYNOPSIS
Builds OpenCV plus the FFmpeg video I/O plugin natively for Windows on ARM64.

.DESCRIPTION
Runs every step of external/README.md: clone OpenCV, unpack a prebuilt winarm64 FFmpeg, configure
and build Release with the ARM64-hosted MSVC toolchain, then deploy the world DLL, the
opencv_videoio_ffmpeg4120 plugin, and FFmpeg's runtime DLLs into external/opencv/arm64/bin.

This build is native-only. On an x64 machine the script stops immediately: cross-compiling is not
supported here.

Completed steps are skipped on re-runs unless -Force is passed.

.EXAMPLE
.\build-opencv-arm64.ps1

.EXAMPLE
.\build-opencv-arm64.ps1 -Force -Generator "Visual Studio 17 2022"
#>

[CmdletBinding()]
param(
    [string]$OpenCvVersion = "4.12.0",

    [string]$FFmpegUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-06-30-13-34/ffmpeg-n7.1.5-1-g7d0e842004-winarm64-gpl-shared-7.1.zip",

    # Empty lets CMake pick the newest installed Visual Studio.
    [string]$Generator = "",

    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Write-Step([string]$Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Skip([string]$Message) {
    Write-Host "    skipped: $Message (use -Force to redo)" -ForegroundColor DarkGray
}

function Invoke-Native {
    param([string]$Exe, [string[]]$Arguments)

    Write-Host "    $Exe $($Arguments -join ' ')" -ForegroundColor DarkGray
    $lines = New-Object System.Collections.Generic.List[string]

    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $Exe @Arguments 2>&1 | ForEach-Object {
            $line = if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.ToString() } else { [string]$_ }
            $lines.Add($line)
            Write-Host $line
        }
    } finally {
        $ErrorActionPreference = $previous
    }

    if ($LASTEXITCODE -ne 0) {
        throw "$Exe exited with code $LASTEXITCODE"
    }
    return ($lines -join [Environment]::NewLine)
}

# PROCESSOR_ARCHITECTURE reports the emulated architecture for a process running under WOW, so the
# ARCHITEW6432 override has to win: an x64 PowerShell on a Windows-on-ARM box is still an ARM host.
function Get-HostArchitecture {
    $candidates = @(
        $env:PROCESSOR_ARCHITEW6432,
        [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString(),
        $env:PROCESSOR_ARCHITECTURE
    )
    foreach ($candidate in $candidates) {
        switch -Regex ($candidate) {
            '^(ARM64|Arm64|AARCH64)$' { return "ARM64" }
            '^(AMD64|X64|x64)$'       { return "x64" }
        }
    }
    return "unknown"
}

function Assert-Arm64Host {
    $arch = Get-HostArchitecture
    if ($arch -ne "ARM64") {
        Write-Host "ERROR: this script only runs on Windows on ARM. Detected host architecture: $arch." -ForegroundColor Red
        Write-Host "OpenCV here is built natively with the ARM64-hosted MSVC toolchain (-A ARM64 -T host=ARM64);" -ForegroundColor Red
        Write-Host "building it on an x64 machine is not supported. Run this on an ARM64 device." -ForegroundColor Red
        exit 1
    }
    if ($env:PROCESSOR_ARCHITEW6432) {
        Write-Warning "Running an emulated $env:PROCESSOR_ARCHITECTURE PowerShell on an ARM64 host. The build itself uses the native ARM64 toolchain, but a native ARM64 shell is faster."
    }
}

function Assert-Tool([string]$Name, [string]$Hint) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name was not found on PATH. $Hint"
    }
}

# CMake wants forward slashes in -D values; backslashes get eaten as escapes in some contexts.
function ConvertTo-CMakePath([string]$Path) {
    return $Path.Replace("\", "/")
}

Assert-Arm64Host

Push-Location $PSScriptRoot
try {
    Assert-Tool -Name "git"   -Hint "Install Git for Windows."
    Assert-Tool -Name "cmake" -Hint "Install CMake 3.20 or newer, or add it to PATH from the Visual Studio installation."

    if ($FFmpegUrl -notmatch "winarm64") {
        throw "FFmpegUrl does not look like a winarm64 package: $FFmpegUrl. An x64 package produces link errors against avcodec.lib."
    }

    $sourceDir  = Join-Path $PSScriptRoot "opencv-$OpenCvVersion"
    $buildDir   = Join-Path $PSScriptRoot "build-opencv"
    $installDir = Join-Path $PSScriptRoot "install-opencv"
    $ffmpegDir  = Join-Path $PSScriptRoot "ffmpeg-arm64"
    $moduleDir  = Join-Path $PSScriptRoot "cmake-modules"
    $deployRoot = Join-Path $PSScriptRoot "opencv\arm64"
    $deployBin  = Join-Path $deployRoot "bin"
    $worldLib   = "opencv_world" + ($OpenCvVersion -replace '\.', '')

    ############################################################################
    Write-Step "Step 1 - Get the sources (OpenCV $OpenCvVersion)"
    ############################################################################
    if ($Force -and (Test-Path $sourceDir)) {
        Remove-Item $sourceDir -Recurse -Force
    }
    if (Test-Path $sourceDir) {
        Write-Skip "$sourceDir already exists"
    } else {
        Invoke-Native git @("clone", "--depth", "1", "--branch", $OpenCvVersion,
                            "https://github.com/opencv/opencv.git", $sourceDir) | Out-Null
    }

    ############################################################################
    Write-Step "Step 2 - Get FFmpeg binaries for Windows on ARM"
    ############################################################################
    if ($Force -and (Test-Path $ffmpegDir)) {
        Remove-Item $ffmpegDir -Recurse -Force
    }
    if (Test-Path (Join-Path $ffmpegDir "lib\avcodec.lib")) {
        Write-Skip "$ffmpegDir already unpacked"
    } else {
        $zip     = Join-Path $PSScriptRoot "ffmpeg-winarm64.zip"
        $staging = Join-Path $PSScriptRoot "ffmpeg-extract"

        Write-Host "    downloading $FFmpegUrl"
        Invoke-WebRequest -Uri $FFmpegUrl -OutFile $zip

        if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
        Expand-Archive $zip -DestinationPath $staging -Force

        # The archives wrap everything in a single versioned directory whose name tracks the release.
        $extracted = @(Get-ChildItem $staging -Directory)
        if ($extracted.Count -ne 1) {
            throw "Expected one top-level directory in the FFmpeg archive, found $($extracted.Count)."
        }
        if (Test-Path $ffmpegDir) { Remove-Item $ffmpegDir -Recurse -Force }
        Move-Item $extracted[0].FullName $ffmpegDir

        Remove-Item $staging -Recurse -Force
        Remove-Item $zip -Force
    }

    foreach ($required in @("include\libavcodec\avcodec.h", "lib\avcodec.lib", "bin")) {
        if (-not (Test-Path (Join-Path $ffmpegDir $required))) {
            throw "FFmpeg package is missing $required. It must be a winarm64 *shared* build."
        }
    }

    ############################################################################
    Write-Step "Step 3 - Configure"
    ############################################################################
    if ($Force -and (Test-Path $buildDir)) {
        Remove-Item $buildDir -Recurse -Force
    }
    if (Test-Path (Join-Path $buildDir "CMakeCache.txt")) {
        Write-Skip "$buildDir is already configured"
    } else {
        $cmakeArgs = @("-S", $sourceDir, "-B", $buildDir, "-A", "ARM64", "-T", "host=ARM64")
        if ($Generator) { $cmakeArgs += @("-G", $Generator) }
        $cmakeArgs += @(
            "-DCMAKE_INSTALL_PREFIX=$(ConvertTo-CMakePath $installDir)",
            "-DCPU_BASELINE=NEON", "-DCPU_DISPATCH=",
            "-DBUILD_SHARED_LIBS=ON", "-DBUILD_opencv_world=ON",
            "-DBUILD_TESTS=OFF", "-DBUILD_PERF_TESTS=OFF", "-DBUILD_EXAMPLES=OFF", "-DBUILD_DOCS=OFF",
            "-DBUILD_opencv_apps=OFF", "-DBUILD_opencv_python3=OFF", "-DBUILD_JAVA=OFF",
            "-DWITH_FFMPEG=ON", "-DOPENCV_FFMPEG_SKIP_DOWNLOAD=ON",
            "-DOPENCV_FFMPEG_USE_FIND_PACKAGE=FFMPEG",
            "-DCMAKE_MODULE_PATH=$(ConvertTo-CMakePath $moduleDir)",
            "-DFFMPEG_ROOT=$(ConvertTo-CMakePath $ffmpegDir)",
            "-DVIDEOIO_PLUGIN_LIST=ffmpeg",
            "-DOPENCV_PLUGIN_VERSION=", "-DOPENCV_PLUGIN_ARCH=", "-DOPENCV_PLUGIN_SUFFIX=4120",
            "-DWITH_GSTREAMER=OFF", "-DWITH_QT=OFF", "-DWITH_1394=OFF",
            "-DWITH_IPP=OFF", "-DBUILD_IPP_IW=OFF", "-DWITH_ITT=OFF", "-DBUILD_ITT=OFF",
            "-DWITH_JASPER=OFF", "-DOPENCV_ENABLE_NONFREE=OFF", "-DWITH_CAROTENE=OFF",
            "-DWITH_CUDA=OFF", "-DWITH_OPENCL=OFF", "-DCMAKE_DISABLE_FIND_PACKAGE_OpenCL=ON",
            "-DWITH_OBSENSOR=OFF"
        )

        $configureLog = Invoke-Native cmake $cmakeArgs

        if ($configureLog -match "Hostx86[/\\]arm64") {
            throw "CMake fell back to the Hostx86 toolchain. Install 'MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools' in the Visual Studio Installer, then re-run with -Force."
        }
        if ($configureLog -notmatch "HostARM64[/\\]arm64") {
            Write-Warning "Could not confirm the HostARM64 compiler in the configure output. Check the 'Check for working CXX compiler' line above."
        }
        if ($configureLog -notmatch "FFMPEG:\s+YES") {
            throw "FFmpeg was not detected. See the Troubleshooting section of README.md."
        }
    }

    ############################################################################
    Write-Step "Step 4 - Build and install (Release)"
    ############################################################################
    Invoke-Native cmake @("--build", $buildDir, "--config", "Release", "--target", "INSTALL", "--parallel") | Out-Null

    $installBin = Join-Path $installDir "bin"
    if (-not (Test-Path $installBin)) {
        throw "No install output at $installBin. Check the build log above."
    }
    $pluginName   = "opencv_videoio_ffmpeg.dll"
    $pluginVerName = "opencv_videoio_ffmpeg4120.dll"
    $worldDll = Join-Path $installBin "$worldLib.dll"
    $pluginDll = Join-Path $installBin $pluginName
    $pluginVerDll = Join-Path $installBin $pluginVerName
    if (-not (Test-Path $worldDll)) {
        throw "$worldLib.dll was not produced."
    }
    if (-not ((Test-Path $pluginDll) -or (Test-Path $pluginVerDll))) {
        throw "Neither $pluginName nor $pluginVerName was produced. The plugin needs -DVIDEOIO_PLUGIN_LIST=ffmpeg and a successful FFmpeg detection."
    }

    ############################################################################
    Write-Step "Step 5 - Deploy to $deployBin"
    ############################################################################
    # One folder: the plugin is only searched for next to the world DLL, and the av* DLLs have to be
    # on the process DLL search path. It is also the DYNAMIC_LIBRARY_DIR that external/CMakeLists.txt
    # hands to the sample apps.
    New-Item -ItemType Directory -Force -Path $deployBin | Out-Null

    Copy-Item (Join-Path $installBin "*.dll") $deployBin -Force
    Copy-Item (Join-Path $ffmpegDir "bin\*.dll") $deployBin -Force

    # Prefer the unversioned install name and rename it; if CMake already emitted the versioned
    # name (OPENCV_PLUGIN_SUFFIX), leave that in place.
    $deployPlugin    = Join-Path $deployBin $pluginName
    $deployPluginVer = Join-Path $deployBin $pluginVerName
    if (Test-Path $deployPlugin) {
        Move-Item -Path $deployPlugin -Destination $deployPluginVer -Force
    } elseif (-not (Test-Path $deployPluginVer)) {
        throw "Expected $pluginName or $pluginVerName in $deployBin after copy."
    }

    Write-Host ""
    Write-Host "Deployed to $deployBin :" -ForegroundColor Green
    Get-ChildItem $deployBin -Filter *.dll | ForEach-Object { Write-Host "    $($_.Name)" }
}
catch {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
