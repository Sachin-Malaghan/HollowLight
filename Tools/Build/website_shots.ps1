# Turns capture screenshots (Tools\Validation\capture.ps1, with and without -Phone) into the website's JPGs.
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."
$src = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$dst = Join-Path $root 'Website\assets\shots'
New-Item -ItemType Directory -Force $dst | Out-Null

$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$params = New-Object System.Drawing.Imaging.EncoderParameters 1
$params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), 84L

function Convert-Shot([string]$name, [string]$out, [int]$width) {
    $path = Join-Path $src "Hollowlight_$name.png"
    if (-not (Test-Path $path)) { Write-Warning "missing $path"; return }
    $img = [System.Drawing.Image]::FromFile($path)
    $h = [int]($img.Height * $width / $img.Width)
    $bmp = New-Object System.Drawing.Bitmap $width, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.DrawImage($img, 0, 0, $width, $h)
    $bmp.Save((Join-Path $dst $out), $codec, $params)
    $g.Dispose(); $bmp.Dispose(); $img.Dispose()
}

Convert-Shot 'desktop_10_level01_log' 'hero.jpg' 1920
Convert-Shot 'desktop_01_title' 'og.jpg' 1200
Convert-Shot 'desktop_01_title' 'title.jpg' 1600
Convert-Shot 'desktop_23_ending' 'ending.jpg' 1600
Convert-Shot 'phone_20_hud_card' 'phone.jpg' 1560
$levels = @('10_level01_log', '11_level02_slope', '12_level03', '13_level04_ladder', '14_level05', '15_level06_racking', '16_level07_wagon', '17_level08_barriers', '18_level09_chasm', '19_level10', '19_level11')
for ($i = 0; $i -lt $levels.Count; $i++) { Convert-Shot ("desktop_" + $levels[$i]) ('level{0:D2}.jpg' -f ($i + 1)) 800 }
Get-ChildItem $dst | Select-Object Name, @{ n = 'KB'; e = { [math]::Round($_.Length / 1KB) } }
