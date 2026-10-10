# Builds a Release copy of the game and packages it as a zip for distribution.
#
# Run from any PowerShell in the repo root, after the usual one-time steps in
# BUILDING.md (N64Recomp built and run, so RecompiledFuncs/ exists, and the ROM
# at the repo root, from which the build generates rsp/). The script sets up Visual Studio's x64 build environment
# itself (a Developer PowerShell often defaults to x86, which can't link x64):
#
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#   .\tools\package-windows.ps1 -Version 0.0.1
#
# Output: dist\DinoTale-<version>-windows.zip containing the exe,
# its DLLs, the assets folder (loaded relative to the working directory, which
# is the exe's folder when launched from Explorer), README, license, and the
# third-party license notices (licenses\).
# Uses its own build folder (build-release) so a Debug dev build is untouched.
param(
    [string]$Version = "beta",
    [string]$BuildDir = "build-release",
    [string]$ClangCl = $env:DT_CLANGCL
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

# Visual Studio's x64 build environment (compiler libraries, Windows SDK, Ninja).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio not found (no vswhere.exe) -- see BUILDING.md." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "No Visual Studio install with the C++ tools found -- see BUILDING.md." }
if ($env:VSCMD_ARG_TGT_ARCH -ne "x64") {
    & (Join-Path $vsPath "Common7\Tools\Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
}
if (-not $ClangCl) { $ClangCl = Join-Path $vsPath "VC\Tools\Llvm\x64\bin\clang-cl.exe" }

Set-Location $root

if (-not (Test-Path "RecompiledFuncs")) {
    throw "RecompiledFuncs\ is missing -- run the N64Recomp step in BUILDING.md first."
}
if (-not (Test-Path "yoshisstory.us.z64")) {
    throw "yoshisstory.us.z64 is missing from the repo root -- the build needs it for the audio microcode."
}
if (-not (Test-Path $ClangCl)) {
    throw "clang-cl not found at $ClangCl -- pass -ClangCl <path> or set `$env:DT_CLANGCL."
}

cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=$ClangCl" "-DCMAKE_CXX_COMPILER=$ClangCl"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmake --build $BuildDir --target DinoTale
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$name = "DinoTale-$Version-windows"
$stage = Join-Path $root "dist\$name"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

foreach ($file in @("DinoTale.exe", "SDL2.dll", "dxil.dll", "dxcompiler.dll")) {
    Copy-Item (Join-Path $BuildDir $file) $stage
}
Copy-Item "assets" $stage -Recurse
Copy-Item "README.md", "COPYING" $stage

# License notices for the third-party code built into the exe or shipped
# beside it. Each library's own file, gathered under licenses\.
$licenseDir = Join-Path $stage "licenses"
New-Item -ItemType Directory -Path $licenseDir | Out-Null
$licenses = [ordered]@{
    "N64ModernRuntime"      = "lib\N64ModernRuntime\COPYING"
    "N64Recomp"             = "lib\N64ModernRuntime\N64Recomp\LICENSE"
    "RT64"                  = "lib\rt64\LICENSE"
    "plume"                 = "lib\rt64\src\contrib\plume\LICENSE"
    "hlslpp"                = "lib\rt64\src\contrib\hlslpp\LICENSE"
    "imgui"                 = "lib\rt64\src\contrib\imgui\LICENSE.txt"
    "implot"                = "lib\rt64\src\contrib\implot\LICENSE"
    "im3d"                  = "lib\rt64\src\contrib\im3d\LICENSE"
    "ddspp"                 = "lib\rt64\src\contrib\ddspp\LICENSE"
    "nativefiledialog-extended" = "lib\rt64\src\contrib\nativefiledialog-extended\LICENSE"
    "stb"                   = "lib\rt64\src\contrib\stb\LICENSE"
    "xxHash"                = "lib\rt64\src\contrib\xxHash\LICENSE"
    "zstd"                  = "lib\rt64\src\contrib\zstd\LICENSE"
    "re-spirv"              = "lib\rt64\src\contrib\re-spirv\LICENSE"
    "miniz"                 = "lib\N64ModernRuntime\thirdparty\miniz\LICENSE"
    "o1heap"                = "lib\N64ModernRuntime\thirdparty\o1heap\LICENSE"
    "RmlUi"                 = "lib\RecompFrontend\recompui\lib\RmlUi\LICENSE.txt"
    "lunasvg"               = "lib\RecompFrontend\recompui\lib\lunasvg\LICENSE"
    "plutovg"               = "lib\RecompFrontend\recompui\lib\lunasvg\plutovg\LICENSE"
    "FreeType"              = "lib\RecompFrontend\recompui\lib\freetype-windows-binaries\LICENSE.TXT"
    "GamepadMotionHelpers"  = "lib\RecompFrontend\lib\GamepadMotionHelpers\LICENSE"
    "SlotMap"               = "lib\SlotMap\README.md"
    "DirectXShaderCompiler" = "licenses\DirectXShaderCompiler.txt"
}
foreach ($entry in $licenses.GetEnumerator()) {
    if (Test-Path $entry.Value) {
        Copy-Item $entry.Value (Join-Path $licenseDir "$($entry.Key).txt")
    }
    else {
        Write-Warning "License file not found: $($entry.Value)"
    }
}
# SDL2 comes from CMake's download, in the build folder.
$sdlLicense = Get-ChildItem -Path (Join-Path $BuildDir "_deps") -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -in @("LICENSE.txt", "COPYING.txt") -and $_.FullName -match "sdl2" } |
    Sort-Object { $_.FullName.Length } | Select-Object -First 1
if ($sdlLicense) { Copy-Item $sdlLicense.FullName (Join-Path $licenseDir "SDL2.txt") }
else { Write-Warning "SDL2 license not found under $BuildDir\_deps" }

$zip = Join-Path $root "dist\$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Write-Host "Packaged $zip"
