# Launches the game (editor build, -game) and runs the -HLCapture script: the autopilot plays through
# every level and screen and screenshots land in Saved/Screenshots/WindowsEditor/Hollowlight_*.png.
#   -Phone   19.5:9 window with the touch controls forced on
param([int]$Width = 1600, [int]$Height = 900, [switch]$Phone)
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$root = Resolve-Path "$PSScriptRoot\..\.."
$project = Join-Path $root 'Hollowlight.uproject'
$extra = @()
$tag = 'desktop'
if ($Phone) { $Width = 1560; $Height = 720; $extra += '-HLForceTouch'; $tag = 'phone' }
& "$engine\Engine\Binaries\Win64\UnrealEditor.exe" "$project" -game -windowed "-ResX=$Width" "-ResY=$Height" -nosplash -unattended `
    -HLCapture "-HLCaptureTag=$tag" @extra | Out-Null
Get-ChildItem (Join-Path $root 'Saved\Screenshots') -Recurse -Filter "Hollowlight_${tag}_*.png" | Select-Object Name, Length
