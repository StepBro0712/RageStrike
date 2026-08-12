$blender = "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
$script  = "C:\Dev\RageStrike\Scripts\bake_arms_for_weapon.py"
$src     = "C:\Dev\RageStrike\ImportSource\Weapons\CS2-Viewmodels"
$arms    = "C:\Dev\RageStrike\ImportSource\Characters\csgo-arms\csgo_arms.glb"
$out     = "C:\Dev\RageStrike\ImportSource\Characters\csgo-arms\baked"

New-Item -ItemType Directory -Force $out | Out-Null

$models = Get-ChildItem $src -Filter "*.glb" | Sort-Object Name
$i = 0
foreach ($m in $models) {
    $i++
    $dst = Join-Path $out ("arms_" + $m.BaseName + ".glb")
    $line = & $blender --background --python $script -- $m.FullName $arms $dst 2>&1 |
            Select-String -Pattern "^RESULT " | Select-Object -First 1
    if ($line) { "[{0,2}/{1}] {2}" -f $i, $models.Count, $line.Line }
    else       { "[{0,2}/{1}] RESULT {2} НЕ ВЫШЛО" -f $i, $models.Count, $m.BaseName }
}

Write-Output "=== готово ==="
Get-ChildItem $out -Filter "*.glb" | Measure-Object Length -Sum | ForEach-Object {
    "файлов: {0}   всего: {1:N0} МБ" -f $_.Count, ($_.Sum/1MB)
}
