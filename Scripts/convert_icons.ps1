# Иконки CS2 лежат в SVG, а Unreal их не читает. Рисуем каждую в PNG через
# headless-браузер (Edge есть в любой Windows 11). SVG узкие и белые, поэтому
# оборачиваем в HTML: масштабируем по размеру кадра и сохраняем прозрачный фон.
$src = "C:\Dev\RageStrike\ImportSource\cs 2 icons\counter-strike-icons-main\cs2\panorama\images\icons\equipment"
$dst = "C:\Dev\RageStrike\ImportSource\GeneratedIcons"
$tmp = Join-Path $env:TEMP "rs_icons"
New-Item -ItemType Directory -Force $dst | Out-Null
New-Item -ItemType Directory -Force $tmp | Out-Null

$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
if (-not (Test-Path $edge)) { $edge = "C:\Program Files\Microsoft\Edge\Application\msedge.exe" }
if (-not (Test-Path $edge)) { Write-Output "Edge not found"; exit 1 }

$map = [ordered]@{
    "Knife"      = "knife.svg"
    "Glock"      = "glock.svg"
    "USP"        = "usp_silencer.svg"
    "P250"       = "p250.svg"
    "Deagle"     = "deagle.svg"
    "Tec9"       = "tec9.svg"
    "FiveSeven"  = "fiveseven.svg"
    "MP9"        = "mp9.svg"
    "MAC10"      = "mac10.svg"
    "UMP45"      = "ump45.svg"
    "P90"        = "p90.svg"
    "Nova"       = "nova.svg"
    "XM1014"     = "xm1014.svg"
    "GalilAR"    = "galilar.svg"
    "FAMAS"      = "famas.svg"
    "AK47"       = "ak47.svg"
    "M4A4"       = "m4a1.svg"
    "AUG"        = "aug.svg"
    "SG553"      = "sg556.svg"
    "SSG08"      = "ssg08.svg"
    "AWP"        = "awp.svg"
    "HEGrenade"  = "hegrenade.svg"
    "Flashbang"  = "flashbang.svg"
    "Smoke"      = "smokegrenade.svg"
    "Molotov"    = "molotov.svg"
    "Incendiary" = "incgrenade.svg"
    "Kevlar"     = "kevlar.svg"
    "Helmet"     = "assaultsuit.svg"
    "C4"         = "c4.svg"
}

$done = 0
foreach ($name in $map.Keys) {
    $file = Join-Path $src $map[$name]
    if (-not (Test-Path $file)) { Write-Output "missing $($map[$name])"; continue }

    # svg рисуем как картинку во всю ширину кадра, фон прозрачный
    $svgUri = "file:///" + ($file -replace '\\', '/' -replace ' ', '%20')
    $html = @"
<html><head><style>
html,body{margin:0;padding:0;background:transparent;width:256px;height:96px;overflow:hidden}
img{position:absolute;left:0;top:0;width:256px;height:96px;object-fit:contain}
</style></head><body><img src="$svgUri"></body></html>
"@
    $page = Join-Path $tmp "$name.html"
    Set-Content -Path $page -Value $html -Encoding UTF8

    $out = Join-Path $dst "Icon_$name.png"
    $pageUri = "file:///" + ($page -replace '\\', '/')

    & $edge --headless --disable-gpu --no-sandbox --hide-scrollbars `
        --default-background-color=00000000 `
        --screenshot="$out" --window-size=256,96 $pageUri 2>$null | Out-Null

    if (Test-Path $out) { $done++ } else { Write-Output "failed: $name" }
}

# отладочный кадр на тёмном фоне: по нему видно, что силуэт нарисовался
$dbgPage = Join-Path $tmp "debug.html"
$akUri = "file:///" + ((Join-Path $src "ak47.svg") -replace '\\', '/' -replace ' ', '%20')
Set-Content -Path $dbgPage -Encoding UTF8 -Value @"
<html><body style="margin:0;background:#203040;width:256px;height:96px">
<img src="$akUri" style="width:256px;height:96px;object-fit:contain"></body></html>
"@
& $edge --headless --disable-gpu --no-sandbox --screenshot="$dst\_debug_dark.png" `
    --window-size=256,96 ("file:///" + ($dbgPage -replace '\\','/')) 2>$null | Out-Null

Write-Output "done: $done icons"
