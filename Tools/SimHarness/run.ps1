# Builds and runs the standalone HOLLOWLIGHT sim harness with MSVC (no Unreal needed).
param([int]$Level = 0, [switch]$Trace)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\..\.."
$out = Join-Path $root 'Intermediate\SimHarness'
New-Item -ItemType Directory -Force $out | Out-Null
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$core = Join-Path $root 'Source\Hollowlight\Private\Core'
$srcs = @("$PSScriptRoot\harness.cpp", "$core\HLSim.cpp", "$core\HLLevels.cpp", "$core\HLAutopilot.cpp") -join '" "'
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && cl /nologo /std:c++17 /O2 /EHsc /W4 /Fo`"$out\\`" /Fe`"$out\harness.exe`" `"$srcs`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "harness build failed" }
$args2 = @()
if ($Level -gt 0) { $args2 += '-level', $Level }
if ($Trace) { $args2 += '-trace' }
& "$out\harness.exe" @args2
exit $LASTEXITCODE
