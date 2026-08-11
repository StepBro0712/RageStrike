$blender = "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
$script  = "C:\Dev\RageStrike\Scripts\convert_viewmodel_one.py"
$src     = "C:\Dev\RageStrike\ImportSource\CS2 Default Weapons Pack DEMO for CSSO by SaViMoN\models\weapons"
$out     = "C:\Dev\RageStrike\ImportSource\Weapons\CS2-Viewmodels"

New-Item -ItemType Directory -Force $out | Out-Null

$models = Get-ChildItem $src -Filter "*.mdl" | Sort-Object Name
$i = 0
foreach ($m in $models) {
    $i++
    $dst = Join-Path $out ($m.BaseName + ".glb")
    $line = & $blender --background --python $script -- $m.FullName $dst 2>&1 |
            Select-String -Pattern "^RESULT " | Select-Object -First 1
    if ($line) { "[{0,2}/{1}] {2}" -f $i, $models.Count, $line.Line }
    else       { "[{0,2}/{1}] RESULT {2} NO OUTPUT" -f $i, $models.Count, $m.BaseName }
}

Write-Output "=== done ==="
Get-ChildItem $out -Filter "*.glb" | Measure-Object Length -Sum | ForEach-Object {
    "files: {0}   total: {1:N0} MB" -f $_.Count, ($_.Sum/1MB)
}
