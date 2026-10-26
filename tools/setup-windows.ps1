# One-time setup for building DinoTale on Windows with Visual Studio 2022.
#
# Run from the repo root in any PowerShell (it sets up Visual Studio's x64
# build environment itself):
#
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#   .\tools\setup-windows.ps1 -Rom "C:\path\to\Yoshi's Story (USA).n64"
#
# What it does:
#   1. fetches the submodules (lib\) if they're missing
#   2. copies your ROM to yoshisstory.us.z64, converting .n64/.v64 byte order,
#      and checks it's the right game
#   3. builds N64Recomp and generates RecompiledFuncs\ from the ROM
#   4. configures the "Windows Release" preset and, unless -NoBuild, builds
#      build\windows-release\DinoTale.exe
#
# Afterwards: open the repo folder in Visual Studio (File > Open > Folder),
# pick the "Windows Release" configuration, select DinoTale.exe as the startup
# item and press F5. Rerun this script (without -Rom) after pulling changes
# to yoshisstory.us.toml or the symbol file, to regenerate RecompiledFuncs\.
#
# Optional: -Icon C:\path\to\icon.png replaces the app icon (icons\app.png)
# and rebuilds icons\app.ico and icons\app.res from it.
param(
    [string]$Rom = "",
    [string]$Icon = "",
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$expectedSha1 = "B13072FEF6C6DF48C07D8822C01E5BC59036F6DA"

function Step($text) { Write-Host "`n== $text" -ForegroundColor Cyan }

# --- Visual Studio x64 environment ---------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio not found (no vswhere.exe) -- see BUILDING.md." }
# Prefer an install that has both the C++ tools and the Clang compiler, so a
# newer install without Clang (another edition, or one an update changed)
# doesn't hide one that has it.
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
if (-not $vsPath) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $vsPath) { throw "No Visual Studio install with the C++ tools found -- see BUILDING.md." }
if ($env:VSCMD_ARG_TGT_ARCH -ne "x64") {
    & (Join-Path $vsPath "Common7\Tools\Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
    Set-Location $root
}
$llvmBin = Join-Path $vsPath "VC\Tools\Llvm\x64\bin"
$clangCl = Join-Path $llvmBin "clang-cl.exe"
if (-not (Test-Path $clangCl)) {
    throw ("clang-cl not found at $clangCl -- in the Visual Studio Installer, Modify this install ($vsPath) and, " +
        "under Individual components, tick 'C++ Clang Compiler for Windows' and 'MSBuild support for LLVM (clang-cl) toolset' (BUILDING.md).")
}
# Make sure the 64-bit clang-cl is the one found by name (the presets use it).
$env:PATH = "$llvmBin;$env:PATH"

# --- 1. Submodules ---------------------------------------------------------
Step "Submodules"
if (-not (Test-Path "lib\N64ModernRuntime\N64Recomp\CMakeLists.txt") -or
    -not (Test-Path "lib\rt64\CMakeLists.txt") -or
    -not (Test-Path "lib\RecompFrontend\CMakeLists.txt")) {
    git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed." }
}
else { Write-Host "present" }

# --- 2. ROM ----------------------------------------------------------------
Step "ROM"
$romOut = Join-Path $root "yoshisstory.us.z64"
if ($Rom) {
    if (-not (Test-Path -LiteralPath $Rom)) { throw "ROM not found: $Rom" }
    $data = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Rom).Path)
    Add-Type -TypeDefinition @"
public static class DtRomSwap {
    public static void Swap16(byte[] d) { for (int i = 0; i + 1 < d.Length; i += 2) { byte t = d[i]; d[i] = d[i + 1]; d[i + 1] = t; } }
    public static void Swap32(byte[] d) { for (int i = 0; i + 3 < d.Length; i += 4) { byte a = d[i], b = d[i + 1]; d[i] = d[i + 3]; d[i + 1] = d[i + 2]; d[i + 2] = b; d[i + 3] = a; } }
}
"@
    $magic = ($data[0..3] | ForEach-Object { $_.ToString("X2") }) -join ""
    switch ($magic) {
        "80371240" { Write-Host "format: .z64 (big-endian)" }
        "37804012" { Write-Host "format: .v64 (byte-swapped), converting"; [DtRomSwap]::Swap16($data) }
        "40123780" { Write-Host "format: .n64 (word-swapped), converting"; [DtRomSwap]::Swap32($data) }
        default { throw "Not an N64 ROM (header bytes $magic)." }
    }
    [System.IO.File]::WriteAllBytes($romOut, $data)
}
if (-not (Test-Path $romOut)) {
    throw "No ROM yet: pass -Rom <your dump> (any of .z64/.n64/.v64)."
}
$sha1 = (Get-FileHash -Algorithm SHA1 $romOut).Hash
if ($sha1 -ne $expectedSha1) {
    throw "yoshisstory.us.z64 has SHA-1 $sha1, expected $expectedSha1 (Yoshi's Story, USA)."
}
Write-Host "yoshisstory.us.z64 OK"

# --- Optional icon -------------------------------------------------------------
if ($Icon) {
    Step "App icon"
    if (-not (Test-Path -LiteralPath $Icon)) { throw "Icon not found: $Icon" }
    Add-Type -AssemblyName System.Drawing
    # Loaded from memory so -Icon can be icons\app.png itself.
    $iconBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Icon).Path)
    $src = [System.Drawing.Image]::FromStream((New-Object System.IO.MemoryStream (, $iconBytes)))
    $src.Save((Join-Path $root "icons\app.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    # .ico with PNG-compressed entries (supported since Windows Vista).
    $sizes = @(256, 128, 64, 48, 32, 24, 16)
    $pngs = foreach ($s in $sizes) {
        $bmp = New-Object System.Drawing.Bitmap $s, $s
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($src, 0, 0, $s, $s)
        $g.Dispose()
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        , $ms.ToArray()
    }
    $src.Dispose()
    $out = New-Object System.IO.MemoryStream
    $w = New-Object System.IO.BinaryWriter $out
    $w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
        $w.Write([Byte]$dim); $w.Write([Byte]$dim); $w.Write([Byte]0); $w.Write([Byte]0)
        $w.Write([UInt16]1); $w.Write([UInt16]32)
        $w.Write([UInt32]$pngs[$i].Length); $w.Write([UInt32]$offset)
        $offset += $pngs[$i].Length
    }
    foreach ($p in $pngs) { $w.Write([byte[]]$p) }
    $w.Flush()
    [System.IO.File]::WriteAllBytes((Join-Path $root "icons\app.ico"), $out.ToArray())
    $rc = Join-Path $root "icons\app.rc"
    Set-Content -Path $rc -Value '1 ICON "app.ico"' -Encoding ascii
    Push-Location (Join-Path $root "icons")
    & (Join-Path $llvmBin "llvm-rc.exe") /FO app.res app.rc
    $rcExit = $LASTEXITCODE
    Pop-Location
    Remove-Item $rc
    if ($rcExit -ne 0) { throw "llvm-rc failed." }
    Write-Host "icons\app.png, app.ico and app.res updated"
}

# --- 3. N64Recomp ------------------------------------------------------------
Step "N64Recomp"
$recompBuild = "lib\N64ModernRuntime\N64Recomp\build"
cmake -S lib\N64ModernRuntime\N64Recomp -B $recompBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=$clangCl" "-DCMAKE_CXX_COMPILER=$clangCl"
if ($LASTEXITCODE -ne 0) { throw "Configuring N64Recomp failed." }
cmake --build $recompBuild --target N64RecompCLI
if ($LASTEXITCODE -ne 0) { throw "Building N64Recomp failed." }
& "$recompBuild\N64Recomp.exe" yoshisstory.us.toml
if ($LASTEXITCODE -ne 0) { throw "N64Recomp failed." }

# --- 4. Configure and build ------------------------------------------------------
Step "DinoTale"
cmake --preset windows-release
if ($LASTEXITCODE -ne 0) { throw "Configuring DinoTale failed." }
if (-not $NoBuild) {
    cmake --build --preset windows-release
    if ($LASTEXITCODE -ne 0) { throw "Building DinoTale failed." }
    Write-Host "`nBuilt build\windows-release\DinoTale.exe" -ForegroundColor Green
}
Write-Host "Open this folder in Visual Studio (File > Open > Folder), choose 'Windows Release', startup item DinoTale.exe, F5."
