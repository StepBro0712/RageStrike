# Иконки killfeed (хедшот, ноускоп, слепой, сквозь дым) лежат в webp, а Unreal
# этот формат не импортирует. Рисуем каждую в PNG через headless-браузер, как
# и SVG-иконки оружия в convert_icons.ps1 — Edge есть в любой Windows 11.
#
# Кадр квадратный: значки killfeed в CS2 близки к квадрату, а object-fit
# сохраняет пропорции внутри него.
$src = "C:\Dev\RageStrike\ImportSource\killfied"
$dst = "C:\Dev\RageStrike\ImportSource\GeneratedIcons"
$tmp = Join-Path $env:TEMP "rs_kficons"
New-Item -ItemType Directory -Force $dst | Out-Null
New-Item -ItemType Directory -Force $tmp | Out-Null

$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
if (-not (Test-Path $edge)) { $edge = "C:\Program Files\Microsoft\Edge\Application\msedge.exe" }
if (-not (Test-Path $edge)) { Write-Output "Edge not found"; exit 1 }

$map = [ordered]@{
    "KFHeadshot"  = "Icon_headshot.webp"
    "KFNoscope"   = "Noscope_kill.webp"
    "KFBlind"     = "Blind_kill.webp"
    "KFSmoke"     = "Smoke_kill.webp"
    "KFPenetrate" = "Csgo_icon-penetrate.webp"
    "KFFlashAst"  = "Flashbang_assist.webp"
}

$done = 0
foreach ($name in $map.Keys) {
    $file = Join-Path $src $map[$name]
    if (-not (Test-Path $file)) { Write-Output "missing $($map[$name])"; continue }

    $uri = "file:///" + ($file -replace '\\', '/' -replace ' ', '%20')
    $html = @"
<html><head><style>
html,body{margin:0;padding:0;background:transparent;width:128px;height:128px;overflow:hidden}
/* Исходники чёрные на прозрачном, а канвас умножает текстуру на цвет:
   чёрный силуэт остался бы чёрным и утонул в тёмной подложке killfeed.
   Инвертируем в белый — как у оружейных иконок, тогда тонировка работает.
   Альфа при invert() не меняется, прозрачность сохраняется. */
img{position:absolute;left:0;top:0;width:128px;height:128px;object-fit:contain;filter:invert(1)}
</style></head><body><img src="$uri"></body></html>
"@
    $page = Join-Path $tmp "$name.html"
    Set-Content -Path $page -Value $html -Encoding UTF8

    $out = Join-Path $dst "$name.png"
    $pageUri = "file:///" + ($page -replace '\\', '/')

    & $edge --headless --disable-gpu --no-sandbox --hide-scrollbars `
        --default-background-color=00000000 `
        --screenshot="$out" --window-size=128,128 $pageUri 2>$null | Out-Null

    if (Test-Path $out) { $done++ } else { Write-Output "failed: $name" }
}

Write-Output "done: $done icons"
