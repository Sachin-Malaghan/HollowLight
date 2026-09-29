# Draws the EMBERHOME app icon (the child with the lantern, in the game's style) and writes every size
# Android, iOS and the website need. Re-run after changing the design; output is committed.
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."

function New-Icon([int]$N, [bool]$Rounded) {
    $bmp = New-Object System.Drawing.Bitmap $N, $N
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $s = $N / 1024.0
    function P([double]$x, [double]$y) { New-Object System.Drawing.PointF ([float]($x * $s)), ([float]($y * $s)) }

    # Sky: light grey at the top to dark grey at the bottom.
    $rect = New-Object System.Drawing.RectangleF 0, 0, $N, $N
    $sky = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, ([System.Drawing.Color]::FromArgb(0xc4, 0xc5, 0xbf)), ([System.Drawing.Color]::FromArgb(0x5e, 0x5f, 0x5a)), 90
    $g.FillRectangle($sky, $rect)

    # Distant trunks in two greys.
    $trunks = @(@(120, 38, 0xa3), @(300, 30, 0xa3), @(760, 34, 0xa3), @(900, 44, 0xa3), @(60, 60, 0x7b), @(640, 54, 0x7b), @(960, 70, 0x7b))
    foreach ($t in $trunks) {
        $b = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb($t[2], $t[2] + 1, $t[2] - 5))
        $g.FillPolygon($b, [System.Drawing.PointF[]]@((P ($t[0] - $t[1] * 0.4) 0), (P ($t[0] + $t[1] * 0.4) 0), (P ($t[0] + $t[1] * 0.6) 1024), (P ($t[0] - $t[1] * 0.6) 1024)))
    }

    # Warm lantern glow behind the child.
    $lx = 600; $ly = 690
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddEllipse([float](($lx - 420) * $s), [float](($ly - 420) * $s), [float](840 * $s), [float](840 * $s))
    $glow = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
    $glow.CenterColor = [System.Drawing.Color]::FromArgb(210, 255, 196, 110)
    $glow.SurroundColors = [System.Drawing.Color[]]@([System.Drawing.Color]::FromArgb(0, 255, 196, 110))
    $g.FillPath($glow, $path)

    $black = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::Black)
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), ([float](22 * $s))
    $pen.StartCap = 'Round'; $pen.EndCap = 'Round'

    # Ground with a grass fringe.
    $g.FillRectangle($black, 0, [float](800 * $s), $N, $N)
    $rnd = New-Object System.Random 7
    for ($x = -10; $x -lt 1034; $x += 14) {
        $h = 18 + $rnd.Next(0, 40)
        $g.FillPolygon($black, [System.Drawing.PointF[]]@((P $x 804), (P ($x + 16) 804), (P ($x + 8 + $rnd.Next(-8, 8)) (804 - $h))))
    }

    # The child: legs, tunic, head with a spiky tuft, scarf trailing behind, arm to the lantern.
    $g.DrawLine($pen, (P 470 640), (P 440 790))
    $g.DrawLine($pen, (P 490 640), (P 530 785))
    $g.FillPolygon($black, [System.Drawing.PointF[]]@((P 455 520), (P 515 520), (P 555 660), (P 410 660)))
    $g.FillEllipse($black, [float](430 * $s), [float](420 * $s), [float](100 * $s), [float](100 * $s))
    $g.FillPolygon($black, [System.Drawing.PointF[]]@((P 468 430), (P 505 426), (P 462 378)))
    $g.FillPolygon($black, [System.Drawing.PointF[]]@((P 440 448), (P 476 426), (P 404 404)))
    $g.FillPolygon($black, [System.Drawing.PointF[]]@((P 498 428), (P 526 446), (P 520 398)))
    $scarf = New-Object System.Drawing.Drawing2D.GraphicsPath
    $scarf.AddBezier((P 470 518), (P 390 490), (P 330 580), (P 230 520))
    $scarf.AddBezier((P 236 548), (P 330 612), (P 400 530), (P 470 550))
    $g.FillPath($black, $scarf)
    $g.DrawLine($pen, (P 510 540), (P 580 610))
    $pen2 = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), ([float](8 * $s))
    $g.DrawLine($pen2, (P 588 612), (P 600 648))

    # Lantern: frame, then a bright amber core.
    $g.FillRectangle($black, [float](568 * $s), [float](648 * $s), [float](64 * $s), [float](12 * $s))
    $g.FillRectangle($black, [float](568 * $s), [float](738 * $s), [float](64 * $s), [float](14 * $s))
    $core = New-Object System.Drawing.Drawing2D.GraphicsPath
    $core.AddEllipse([float](560 * $s), [float](640 * $s), [float](80 * $s), [float](110 * $s))
    $cb = New-Object System.Drawing.Drawing2D.PathGradientBrush $core
    $cb.CenterColor = [System.Drawing.Color]::FromArgb(255, 255, 244, 214)
    $cb.SurroundColors = [System.Drawing.Color[]]@([System.Drawing.Color]::FromArgb(255, 255, 176, 80))
    $g.FillEllipse($cb, [float](572 * $s), [float](660 * $s), [float](56 * $s), [float](78 * $s))
    $g.FillRectangle($black, [float](568 * $s), [float](660 * $s), [float](8 * $s), [float](80 * $s))
    $g.FillRectangle($black, [float](624 * $s), [float](660 * $s), [float](8 * $s), [float](80 * $s))

    $g.Dispose()
    return $bmp
}

function Save([System.Drawing.Bitmap]$src, [int]$size, [string]$path) {
    New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
    $dst = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.DrawImage($src, 0, 0, $size, $size)
    $g.Dispose()
    $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
}

$icon = New-Icon 1024 $false

# Android (legacy launcher icons; UE copies Build/Android/res over its defaults)
$android = @{ 'drawable-ldpi' = 36; 'drawable-mdpi' = 48; 'drawable-hdpi' = 72; 'drawable-xhdpi' = 96; 'drawable-xxhdpi' = 144; 'drawable-xxxhdpi' = 192; 'drawable' = 192 }
foreach ($k in $android.Keys) { Save $icon $android[$k] (Join-Path $root "Build\Android\res\$k\icon.png") }
Save $icon 512 (Join-Path $root 'Build\Android\PlayStore\icon-512.png')

# iOS: single 1024 icon in the asset catalog (Xcode 14+), plus the legacy Graphics folder
$appicon = Join-Path $root 'Build\IOS\Resources\Assets.xcassets\AppIcon.appiconset'
Save $icon 1024 (Join-Path $appicon 'Icon1024.png')
Set-Content (Join-Path $appicon 'Contents.json') -Encoding ascii -Value @'
{
  "images" : [ { "filename" : "Icon1024.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" } ],
  "info" : { "author" : "xcode", "version" : 1 }
}
'@
Set-Content (Join-Path $root 'Build\IOS\Resources\Assets.xcassets\Contents.json') -Encoding ascii -Value '{ "info" : { "author" : "xcode", "version" : 1 } }'
Save $icon 1024 (Join-Path $root 'Build\IOS\Resources\Graphics\Icon1024.png')

# Website
Save $icon 512 (Join-Path $root 'Website\assets\icon-512.png')
Save $icon 180 (Join-Path $root 'Website\assets\apple-touch-icon.png')
Save $icon 32 (Join-Path $root 'Website\assets\favicon-32.png')
$icon.Dispose()
'icons written'
