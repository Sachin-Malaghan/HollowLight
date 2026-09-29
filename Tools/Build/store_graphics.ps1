# Builds Google Play store graphics from the capture screenshots (Tools\Validation\capture.ps1 [-Phone]).
# Output: Build/Android/PlayStore/ (feature graphic 1024x500, screenshots <= 2:1, icon 512).
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."
$src = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$out = Join-Path $root 'Build\Android\PlayStore'
New-Item -ItemType Directory -Force $out | Out-Null

$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$q = New-Object System.Drawing.Imaging.EncoderParameters 1
$q.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), 92L

function Load([string]$name) {
    $p = Join-Path $src "Hollowlight_$name.png"
    if (-not (Test-Path $p)) { throw "missing screenshot $p - run Tools\Validation\capture.ps1 first" }
    [System.Drawing.Image]::FromFile($p)
}

# Draws $img into a WxH frame, cropping to fill (focus = 0..1 horizontal / vertical focus point).
function Fill([System.Drawing.Image]$img, [int]$W, [int]$H, [double]$fx = 0.5, [double]$fy = 0.5) {
    $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'
    $s = [Math]::Max($W / $img.Width, $H / $img.Height)
    $sw = $W / $s; $sh = $H / $s
    $sx = ($img.Width - $sw) * $fx; $sy = ($img.Height - $sh) * $fy
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), [float]$sx, [float]$sy, [float]$sw, [float]$sh, 'Pixel')
    return @($bmp, $g)
}

# Letter-spaced text centred at (cx, cy).
function Tracked($g, [string]$text, $font, $brush, [double]$cx, [double]$cy, [double]$tracking) {
    $fmt = [System.Drawing.StringFormat]::GenericTypographic
    $widths = foreach ($ch in $text.ToCharArray()) { $g.MeasureString([string]$ch, $font, 10000, $fmt).Width }
    $total = ($widths | Measure-Object -Sum).Sum + $tracking * ($text.Length - 1)
    $x = $cx - $total / 2
    $h = $g.MeasureString($text, $font, 10000, $fmt).Height
    for ($i = 0; $i -lt $text.Length; $i++) {
        $g.DrawString([string]$text[$i], $font, $brush, [float]$x, [float]($cy - $h / 2), $fmt)
        $x += $widths[$i] + $tracking
    }
}

# ---- Feature graphic 1024 x 500 ----
$img = Load 'desktop_10_level01_log'
$r = Fill $img 1024 500 0.42 0.62
$bmp = $r[0]; $g = $r[1]
$shade = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Rectangle 0, 0, 1024, 500), ([System.Drawing.Color]::FromArgb(150, 0, 0, 0)), ([System.Drawing.Color]::FromArgb(40, 0, 0, 0)), 90
$g.FillRectangle($shade, 0, 0, 1024, 500)
$serif = 'Palatino Linotype'
if (-not ([System.Drawing.FontFamily]::Families | Where-Object Name -eq $serif)) { $serif = 'Georgia' }
$title = New-Object System.Drawing.Font $serif, 64, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$sub = New-Object System.Drawing.Font 'Segoe UI Light', 20, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$ink = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(0xeb, 0xe7, 0xdc))
$dim = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(0xc8, 0xc4, 0xb8))
$shadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(140, 0, 0, 0))
Tracked $g 'HOLLOWLIGHT' $title $shadow 514 132 22
Tracked $g 'HOLLOWLIGHT' $title $ink 512 130 22
Tracked $g 'bring the light home' $sub $dim 512 196 9
$g.Dispose(); $img.Dispose()
$bmp.Save((Join-Path $out 'feature-graphic-1024x500.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

# ---- Phone screenshots (16:9 and 2:1, both within Play's 2:1 limit) ----
$shots = [ordered]@{
    '01-title' = @('desktop_01_title', 1600, 900)
    '02-edge-of-the-wood' = @('desktop_10_level01_log', 1600, 900)
    '03-touch-controls' = @('phone_20_hud_card', 1440, 720)
    '04-teeth-in-the-grass' = @('desktop_12_level03', 1600, 900)
    '05-drifting-boughs' = @('desktop_14_level05', 1600, 900)
    '06-still-water' = @('desktop_16_level07', 1600, 900)
    '07-the-storm' = @('desktop_18_level09', 1600, 900)
    '08-homecoming' = @('desktop_23_ending', 1600, 900)
}
foreach ($k in $shots.Keys) {
    $s = $shots[$k]
    $img = Load $s[0]
    $r = Fill $img $s[1] $s[2] 0.5 0.5
    $r[1].Dispose(); $img.Dispose()
    $r[0].Save((Join-Path $out "screenshot-$k.jpg"), $jpeg, $q)
    $r[0].Dispose()
}

Get-ChildItem $out | Select-Object Name, @{ n = 'KB'; e = { [math]::Round($_.Length / 1KB) } }
