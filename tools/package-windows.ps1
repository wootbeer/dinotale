# Builds the Release game and packages it as a zip for distribution.
#
# Run from any PowerShell in the repo root, after the one-time setup in
# BUILDING.md (your ROM already copied in by tools\setup-windows.ps1 -Rom):
#
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#   .\tools\package-windows.ps1 -Version 0.1.5
#
# It builds through tools\setup-windows.ps1 (the same build you play), then
# stages the exe, its DLLs, the assets folder, README, CHANGELOG, license and
# the third-party license notices (licenses\) into
# dist\DinoTale-<version>-windows.zip. Nothing from the ROM other than the
# recompiled code built into the exe is included.
param(
    [string]$Version = "0.1.5"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

& (Join-Path $PSScriptRoot "setup-windows.ps1")
Set-Location $root
$BuildDir = "build\windows-release"

$name = "DinoTale-$Version-windows"
$stage = Join-Path $root "dist\$name"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item (Join-Path $BuildDir "DinoTale.exe") $stage
foreach ($file in @("SDL2.dll", "dxil.dll", "dxcompiler.dll")) {
    $path = Join-Path $BuildDir $file
    if (Test-Path $path) { Copy-Item $path $stage }
    else { Write-Warning "$file not found in $BuildDir" }
}
Copy-Item "assets" $stage -Recurse
Copy-Item "README.md", "CHANGELOG.md", "COPYING" $stage

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
    "SlotMap"               = "lib\RecompFrontend\lib\SlotMap\slot_map.h"
    "DirectXShaderCompiler" = "licenses\DirectXShaderCompiler.txt"
    "LZMA-SDK"              = "thirdparty\lzma-sdk\lzma-sdk.txt"
    "screen-filters"        = "licenses\screen-filters.txt"
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
