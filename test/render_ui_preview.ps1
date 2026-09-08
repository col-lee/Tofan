# Render production drawing commands with System.Drawing (desktop font approximation).
Add-Type -AssemblyName System.Drawing
$previewRoot = Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/build/ui-preview'
function Color565([int]$value) {
    return [Drawing.Color]::FromArgb((($value -shr 11) -band 31)*255/31, (($value -shr 5) -band 63)*255/63, ($value -band 31)*255/31)
}
$sheet = [Drawing.Bitmap]::new(1344, 1120)
$canvas = [Drawing.Graphics]::FromImage($sheet)
$canvas.Clear([Drawing.Color]::FromArgb(224,230,227))
$labelFont = [Drawing.Font]::new('Segoe UI',12,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
$index = 0
foreach ($file in (Get-ChildItem -LiteralPath $previewRoot -Filter '*.jsonl' | Sort-Object Name)) {
    $bitmap = [Drawing.Bitmap]::new(640,480)
    $g = [Drawing.Graphics]::FromImage($bitmap)
    $g.ScaleTransform(2,2)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    foreach ($line in [IO.File]::ReadLines($file.FullName)) {
        $op = $line | ConvertFrom-Json
        [single[]]$a = $op.a
        $color = Color565 $a[$a.Length-1]
        $brush = [Drawing.SolidBrush]::new($color)
        $pen = [Drawing.Pen]::new($color,1)
        switch ($op.op) {
            'rect' { $g.FillRectangle($brush,$a[0],$a[1],$a[2],$a[3]) }
            'outline' { $g.DrawRectangle($pen,$a[0],$a[1],$a[2],$a[3]) }
            {$_ -in 'round','round-outline'} {
                $r = [Math]::Min($a[4],[Math]::Min($a[2],$a[3])/2)
                if ($r -gt 0) {
                    $path=[Drawing.Drawing2D.GraphicsPath]::new()
                    $d=[single]($r*2)
                    $path.AddArc($a[0],$a[1],$d,$d,180,90)
                    $path.AddArc($a[0]+$a[2]-$d,$a[1],$d,$d,270,90)
                    $path.AddArc($a[0]+$a[2]-$d,$a[1]+$a[3]-$d,$d,$d,0,90)
                    $path.AddArc($a[0],$a[1]+$a[3]-$d,$d,$d,90,90)
                    $path.CloseFigure()
                    if ($op.op -eq 'round') { $g.FillPath($brush,$path) } else { $g.DrawPath($pen,$path) }
                    $path.Dispose()
                }
            }
            'ellipse' { $g.FillEllipse($brush,$a[0],$a[1],$a[2],$a[3]) }
            'ellipse-outline' { $g.DrawEllipse($pen,$a[0],$a[1],$a[2],$a[3]) }
            'line' { $g.DrawLine($pen,$a[0],$a[1],$a[2],$a[3]) }
            'triangle' {
                [Drawing.PointF[]]$points=@([Drawing.PointF]::new($a[0],$a[1]),[Drawing.PointF]::new($a[2],$a[3]),[Drawing.PointF]::new($a[4],$a[5]))
                $g.FillPolygon($brush,$points)
            }
            'text' {
                $size = switch ([int]$a[2]) { 1 {8} 2 {14} 4 {24} 6 {48} default {12} }
                $font=[Drawing.Font]::new('Segoe UI',$size,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
                $format=[Drawing.StringFormat]::GenericTypographic
                $measure=$g.MeasureString([string]$op.text,$font,1000,$format)
                [single]$x=$a[0]; [single]$y=$a[1]
                $col=[int]$a[3]%3; $row=[Math]::Floor($a[3]/3)
                if ($col -eq 1) {$x-=$measure.Width/2} elseif ($col -eq 2) {$x-=$measure.Width}
                if ($row -eq 1) {$y-=$measure.Height/2} elseif ($row -eq 2) {$y-=$measure.Height}
                $g.DrawString([string]$op.text,$font,$brush,$x,$y,$format)
                $font.Dispose()
            }
        }
        $brush.Dispose(); $pen.Dispose()
    }
    $bitmap.Save((Join-Path $previewRoot ($file.BaseName+'.png')),[Drawing.Imaging.ImageFormat]::Png)
    $x=8+($index%4)*336; $y=8+[Math]::Floor($index/4)*280
    $canvas.DrawString($file.BaseName,$labelFont,[Drawing.Brushes]::Black,$x,$y)
    $canvas.DrawImage($bitmap,[int]$x,[int]($y+24),320,240)
    $g.Dispose();$bitmap.Dispose();$index++
}
$sheet.Save((Join-Path $previewRoot 'contact-sheet.png'),[Drawing.Imaging.ImageFormat]::Png)
$canvas.Dispose();$sheet.Dispose();$labelFont.Dispose()
Write-Output "Rendered $index production UI layouts (desktop font approximation)."
