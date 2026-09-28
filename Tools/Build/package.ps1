# Packages HOLLOWLIGHT with Unreal's BuildCookRun.
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Win64
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android            # needs Android Studio + SDK/NDK (SETUP.md)
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Release   # signed .aab for Google Play
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform IOS                # needs a Mac with Xcode (remote build, SETUP.md)
# Output: Packaged/<Platform>/
param(
    [ValidateSet('Win64', 'Android', 'IOS')][string]$Platform = 'Win64',
    [ValidateSet('Shipping', 'Development')][string]$Config = 'Shipping',
    [switch]$Release
)
$ErrorActionPreference = 'Stop'
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$root = Resolve-Path "$PSScriptRoot\..\.."
$project = Join-Path $root 'Hollowlight.uproject'
$out = Join-Path $root "Packaged\$Platform"

$args = @(
    'BuildCookRun', "-project=$project", '-noP4', "-platform=$Platform", "-clientconfig=$Config",
    '-build', '-cook', '-stage', '-pak', '-iostore', '-compressed', '-archive', "-archivedirectory=$out",
    '-nodebuginfo', '-utf8output', '-unattended'
)
switch ($Platform) {
    'Win64'   { $args += '-prereqs' }
    'Android' { $args += '-cookflavor=ASTC'; if ($Release) { $args += '-distribution' } }
    'IOS'     { if ($Release) { $args += '-distribution' } }
}
& "$engine\Engine\Build\BatchFiles\RunUAT.bat" @args
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed ($LASTEXITCODE)" }
Get-ChildItem $out -Recurse -Include *.exe, *.apk, *.aab, *.ipa -ErrorAction SilentlyContinue | Select-Object FullName, Length
