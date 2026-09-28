# Builds the editor target and runs every Hollowlight automation test headless; prints a summary.
param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$root = Resolve-Path "$PSScriptRoot\..\.."
$project = Join-Path $root 'Hollowlight.uproject'
if (-not $NoBuild) {
    & "$engine\Engine\Build\BatchFiles\Build.bat" HollowlightEditor Win64 Development -project="$project" -waitmutex | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
}
$log = Join-Path $root 'Saved\Logs\Hollowlight.log'
& "$engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$project" -nullrhi -unattended -nosplash -nopause `
    -ExecCmds="Automation RunTests Hollowlight; Quit" -TestExit="Automation Test Queue Empty" | Out-Null
$results = Select-String -Path $log -Pattern 'Test Completed\. Result=\{(\w+)\} Name=\{(\w+)\} Path=\{([^}]+)\}'
$fail = 0
foreach ($r in $results) {
    $ok = $r.Matches[0].Groups[1].Value -eq 'Success'
    if (-not $ok) { $fail++ }
    '{0}  {1}' -f ($(if ($ok) { 'PASS' } else { 'FAIL' })), $r.Matches[0].Groups[3].Value
}
Select-String -Path $log -Pattern 'LogAutomationController: Error: (?!Test Completed)' | ForEach-Object { '      ' + $_.Line.Split('Error: ')[-1] }
if ($results.Count -eq 0) { 'No test results found in ' + $log; exit 2 }
"{0} passed, {1} failed" -f ($results.Count - $fail), $fail
exit $fail
